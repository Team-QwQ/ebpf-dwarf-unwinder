# bcc/libbpf-tools memleak 适配示例规范

## 背景
- `examples/bpf_memleak/` 目前提供了极简的 ring buffer → `dwunw_capture` 演示，但其 BPF/用户态代码与真实的 `src/ref/bcc/libbpf-tools/memleak*` 差距较大，无法直接反映在 upstream 工具里植入 DWARF unwinder 的具体改动点。
- 为方便评审与后续 PR，需要一份“贴近实战”的示例：以 upstream memleak 代码为基础（保持文件结构与命名习惯），仅在必要位置接入 `libdwunw`，并用注释突出差异，供 memleak 维护者快速理解。
- 同时需要避免与现有 `examples/bpf_memleak/` 名称冲突，以免混淆不同层级的示例定位。

## 目标
1. 新增一个示例目录（命名与现有 `bpf_memleak` 区分，例如 `examples/memleak_bcc_dwunw/`），其中包含：
   - `memleak_dwunw.bpf.c`（基于 upstream `memleak.bpf.c`）
   - `memleak_dwunw_user.c`（基于 upstream `memleak.c`）
   - `memleak_dwunw.h`/`README.md` 等必要文件
2. Upstream 原始代码段保持原样，凡是引入 `libdwunw` 或为其配套的逻辑，均以统一注释标识（例如 `/* dwunw-added: ... */` 或 `// dwunw-added ...`），方便 reviewers diff。
3. 新示例必须能：
   - 通过 `libbpf` 装载 BPF 程序，监听与 upstream 相同的 uprobe/kprobe 点；
   - 将 memleak 事件中的分配栈解析交给 `dwunw_capture`（至少支持用户态栈解析，允许保留原有 perf map 解析逻辑作为对照）；
   - 提供 CLI 开关以启用/禁用 DWARF 路径，默认开启。
4. `make examples` 中新增对应 target（例如 `memleak_dwunw_user`），默认不影响现有 `memleak_user` 产物。
5. 在 README/文档中描述示例的来源、命名约定与注释风格，指导如何与 upstream diff。

## 非目标
- 不重写或替换 upstream `src/ref/bcc` 目录；示例位于本仓库 `examples/` 下。
- 不尝试 upstream 合并流程；示例仅为演示/验证。
- 不改变原有 `examples/bpf_memleak/` 的内容或构建方式。

## 详细需求
### 命名与目录
- 目录名建议 `examples/memleak_bcc_dwunw/`，文件命名采用 `memleak_dwunw_*.{c,h}` 以免与 upstream 文件混淆。
- README 顶部需声明“基于 `src/ref/bcc/libbpf-tools/memleak*` 改造”并说明 diff 方式。

### 代码来源与同步策略
- 初始内容直接拷贝 `src/ref/bcc/libbpf-tools/memleak*.{c,h}`，保留版权与 SPDX 注释。
- 需记录对应 upstream commit（当前为 `bcc@8d85dcfac86b`）并在 README 中注明，如需更新必须重新同步并验证。

### DWUNW 接入点
- 用户态：
  - 引入 `dwunw/dwunw_api.h`、`dwunw/unwind.h`，在 memleak 的栈输出路径新增 DWARF 解析。
  - 当 `libdwunw` 启用时，绕过 `ksyms/syms_cache` 解析，改为对每条事件调用 `dwunw_capture` 生成帧列表；可提供 CLI 参数 `--dwunw-mode=off|fallback|force`。
  - 若 DWARF 不可用，应保留原有回退逻辑并打印提示。
- BPF：
  - 保留原有 ring buffer 与 stack 保存逻辑，但需确保事件结构能携带足够寄存器/模块信息以供 `dwunw_capture` 使用。
  - 如需新增字段（例如 `struct memleak_event_dwunw`），同样加注释标识新增来源。

### 注释规范
- 所有新代码、结构体字段、函数或宏需以 `/* dwunw-added: <一句话> */` 或 `// dwunw-added: ...` 紧邻标记。
- 若对原行进行修改（而非新增），需在行尾追加 `// dwunw-added` 或使用多行块注释解释原因。
- README 需用表格或列表列出“dwunw 标记含义与搜索方式”。

### 构建与脚本
- `Makefile` `examples` 目标中加入新产物：`build/$(ARCH)/examples/memleak_bcc_dwunw/memleak_dwunw_user`。
- 允许通过 `LIBBPF_CFLAGS`/`LIBBPF_LDLIBS` 复用已有环境变量，必要时新增 `DWUNW_EXAMPLE_CFLAGS` 以容纳 `_GNU_SOURCE` 等宏。

### 文档
- `examples/memleak_bcc_dwunw/README.md`：
  - 源代码出处、使用步骤、所需环境变量；
  - 如何比较与 upstream 的差异（例如 `diff -u src/ref/bcc/libbpf-tools/memleak.c examples/.../memleak_dwunw_user.c | grep dwunw-added`）。
- `doc/submodule_versions.md` 可引用该示例作为 bcc 依赖的验证用例（无需在本规范中修改文档，但后续实现需同步）。

### 测试与验证
- `make examples DWUNW_MEMLEAK_BCC=1`（或默认）能成功构建新的用户态程序。
- 提供最少一个本地验证脚本/段落：
  - 在无 DWARF 时观察 fallback 输出；
  - 在有 DWARF 的测试 ELF（可沿用 fixtures）下打印 `dwunw_capture` 结果。
- 不新增自动化单元测试，但要求在 README 中记录手动验证命令。

## 验收标准
1. 新目录/文件存在且保持注释规范，`grep -R "dwunw-added" examples/memleak_bcc_dwunw` 能命中所有改动点。
2. README 说明清晰，包含源 commit、注释约定、构建运行步骤。
3. `make examples` 能在 x86_64 环境产出 `memleak_dwunw_user`，并与 `libdwunw.a`、`libbpf` 成功链接。
4. 示例运行时可通过 CLI 切换 DWARF 解析与传统解析，两种路径均能输出栈信息（允许示例中使用 mock/fixture 触发）。
5. 不破坏现有 `examples/bpf_memleak` 与测试目标。

## 风险与缓解
- **upstream 代码体量大**：通过注释标识+README diff 指南，降低评审难度；必要时在 README 提供 `git diff --word-diff` 示例。
- **同步成本**：记录上游 commit，若后续 memleak 更新需重新拷贝并重新应用注释；可考虑脚本化 diff。
- **构建依赖膨胀**：示例默认沿用 `libbpf` 依赖，未额外引入第三方库，确保 `make examples` 仅增加一个可执行文件。
