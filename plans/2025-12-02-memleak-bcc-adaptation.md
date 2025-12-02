# 计划：bcc memleak DWUNW 适配示例

## 概述
- **范围**：依据 `specs/2025-12-02-memleak-bcc-adaptation.md`，在 `examples/` 下新增 `memleak_bcc_dwunw/` 目录，复制 `src/ref/bcc/libbpf-tools/memleak*.{c,h}` 并注入 `libdwunw` 支持，确保构建目标、README、注释标记与运行路径就绪。
- **目标**：提供一个贴近 upstream memleak 的示例，展示如何以“差异可视 + 可切换”方式集成 DWARF unwinder，同时保持原有 `examples/bpf_memleak/` 不受影响。

## 关联规范
- `specs/2025-12-02-memleak-bcc-adaptation.md`

## 假设与约束
- bcc 子模块已固定在 `8d85dcfac86b`（v0.32.0）；示例以该版本为基线。
- `libbpf`、`libdwunw` 已可通过 `make examples` 构建；示例沿用相同的 `LIBBPF_CFLAGS/LIBBPF_LDLIBS` 接口。
- 仅在新目录内操作；`src/ref/bcc` 与现有 `examples/bpf_memleak` 禁止改动。

## 分阶段计划
1. **阶段A：目录与代码同步**
   - 创建 `examples/memleak_bcc_dwunw/`，拷贝 `memleak.bpf.c`、`memleak.c`、`memleak.h`、`trace_helpers.*`（若依赖）等必要文件。
   - 保留原版权/SPDX 注释，记录引用 commit。
   - 在 README 中列出“如何 diff 与 upstream”。

2. **阶段B：DWUNW 接入与注释标记**
   - 用户态 `memleak_dwunw_user.c`：
     - 引入 `dwunw` 头文件、新增 CLI 参数（`--dwunw-mode`）。
     - 在事件消费路径中加入 `dwunw_capture`，回退逻辑沿用原 `ksyms/syms_cache`。
     - 所有新增或修改段落加 `dwunw-added` 注释。
   - BPF 侧 `memleak_dwunw.bpf.c`：若需扩展事件结构/元数据，同样按注释规范标记。
   - 公共头文件 `memleak_dwunw.h`：增加 DWARF 所需字段与注释。

3. **阶段C：构建与 README 更新**
   - 修改顶层 `Makefile`，在 `examples` 目标中生成 `build/$(ARCH)/examples/memleak_bcc_dwunw/memleak_dwunw_user`。
   - 编写 `examples/memleak_bcc_dwunw/README.md`，包含依赖、构建命令、`dwunw-added` 搜索指南。

4. **阶段D：验证**
   - `make examples LIBBPF_CFLAGS=... LIBBPF_LDLIBS=...`，确认新目标与原目标共存。
   - 运行 `memleak_dwunw_user`：
     - `--dwunw-mode=force` 与 `--dwunw-mode=off` 两种模式，验证 DWARF 路径/回退路径输出。
     - 若需 mock，可记录演示命令（例如与 `tests/fixtures/dwarf_fixture` 结合）。
   - `make test` 确认全局构建未受影响。

## 影响与依赖
- 新增文件：`examples/memleak_bcc_dwunw/*`、`README.md`。
- 修改文件：`Makefile`（新增示例 target）、可选 `doc/README`（如需引用新示例）。
- 依赖：libbpf、libdwunw、gcc/clang、bpftool。

## 风险与缓解
- **拷贝 upstream 体积大**：在 README 标明来源与 commit，后续 diff 通过 `dwunw-added` 注释过滤。
- **构建时间增加**：示例仅增加一个可执行文件，且复用现有依赖，影响有限。
- **功能验证难**：提供 CLI fallback，确保即使 DWARF 不可用也能运行；README 写明验证步骤。

## 验证策略
- `make examples`；
- `make test`；
- 运行 `build/<arch>/examples/memleak_bcc_dwunw/memleak_dwunw_user --dwunw-mode=force ...` 与 `--dwunw-mode=off ...`；
- grep `dwunw-added` 确认注释覆盖。

## 审批状态
- 当前阶段：`/plan`
- 下一步：待确认后进入 `/do`。
