# 计划：DWARF eBPF 栈回溯库

## 概述
- **范围**：实现 `specs/2025-12-01-dwarf-ebpf-stack-unwinder.md` 中定义的静态库、示例与文档，重点支持 `libbpf-tools/memleak` 的 eBPF 场景，并在最新阶段扩展到“完整 DWARF 栈展开（多帧回溯）”。
- **目标**：在不修改内核的前提下，提供跨架构、可插拔的 DWARF 栈回溯实现，先保证根帧，再完成 FDE/CFI 解析与多帧输出，同时给出与 memleak 集成的可验证示例。
- **交付物**：`libdwunw.a`、API/架构文档、`examples/bpf_memleak/` 与 `examples/memleak_bcc_dwunw/` 示例、针对 x86_64/arm64/mips32 的测试资产以及新增的多帧回溯测试工具。

## 依赖与前置条件
1. 现有子模块：`src/ref/ghostscope`、`src/ref/parca`、`src/ref/bcc`（v0.32.0）可作为参考，避免直接复制代码。
2. 构建工具链：固定使用 GNU Make，Makefile 需覆盖 x86_64/arm64/mips32 目标；DWARF 解析阶段可使用 clang/llvm 工具，但最终交付的静态库必须能在缺少 clang/llvm 的环境中直接链接。
3. 运行环境：Linux (kernel >= 5.4) 以便加载 eBPF 程序；用户态示例需要 perf/ring buffer 支持。

## 假设与范围界定
- 仅实现用户态库，不在内核空间部署逻辑。
- 示例聚焦 memleak 工作流；不提供独立的 Parca 对接或纯用户态（无 eBPF）示例。
- DWARF 输入认为来自带调试符号的 ELF 或独立调试文件；符号缺失时以错误码返回。
- 采用 C 语言及最小 libc 依赖；不使用 C++/Rust。

## 实施阶段
1. **仓库结构与构建基线**（已完成）
   - 创建 `src/` 及子目录（`src/core`, `src/dwarf`, `src/arch/<arch>` 等），建立统一的 `include/`。
   - 以 GNU Make 为核心构建系统：顶层 `Makefile` 提供库、测试、示例目标；拆分 `mk/toolchain.mk` 描述交叉编译前缀；通过 `make ARCH=arm64` 等参数切换架构，无需 CMake 依赖。
   - 引入基础 `config.h`/`dwunw_api.h`，定义公共类型、错误码骨架。

2. **平台抽象层 (PAL)**（已完成）
   - 设计 `struct dwunw_arch_ops` 接口（寄存器布局、CFA 策略、返回地址提取）。
   - 提供 x86_64 可用实现，arm64/mips32 stub，并以 `tests/unit/test_arch_ops` 验证接口覆盖度。
   - 定义包含架构标签、版本号的寄存器快照结构，兼容 BPF 事件承载格式。

3. **DWARF/ELF 装载与索引**（已完成）
   - 提供路径驱动的 ELF 载入器：校验 ELF 头、抓取 section table、收集 `.debug_info/.debug_frame/.eh_frame` 数据。
   - 实现 DWARF 索引骨架 `dwunw_dwarf_index`，可惰性收集调试段并暴露缓存结构，解析逻辑待阶段 4 扩展。
   - 引入模块缓存 `dwunw_module_cache`，基于路径复用 ELF/DWARF 句柄，配套单元测试验证无符号/命中/释放路径。

4. **栈回溯引擎**（已完成）
   - 提供 `dwunw_capture` 主循环，整合 `dwunw_module_cache`、`dwunw_arch_ops` 与寄存器快照；能够输出至少首帧并标记部分帧状态。
   - 为 CFA/RA 解算提供接口骨架：当架构或 DWARF 能力不足时以 `DWUNW_FRAME_FLAG_PARTIAL` 标记，并返回细粒度错误码。
   - 引入 `tests/unit/test_unwinder`，验证模块缓存依赖、输入参数校验及单帧生成逻辑。

5. **eBPF 事件集成 & 示例**（已完成）
   - `examples/bpf_memleak/` 提供共享事件结构、`memleak_bpf.c`（kprobe do_exit 样例）与 `memleak_user.c`（libbpf loader），串联 ring buffer → `dwunw_capture`。
   - BPF 事件结构可投递跨架构寄存器快照，用户态使用 `memleak_event_to_regset()` 转换并写入模块缓存。
   - README（中文）覆盖 clang/llvm 仅用于 BPF 编译、libbpf 构建示例、运行/验证步骤及扩展建议。

6. **跨架构支持扩展**（已完成）
   - 基于阶段 4 的框架补全 arm64/mips32 ops，实现寄存器映射与 CFI 适配。
   - 更新 `tests/unit/test_arch_ops` 验证 FP/LR、RA 退化路径，确保两个架构都能计算 CFA 并给出返回地址。
   - 使用预制的 ELF 样例或交叉编译测试二进制，验证 `dwunw_capture` 能产生正确帧序列。

7. **测试与文档完善**（已完成）
   - `tests/unit/`: 新增 `test_dwarf_index` 覆盖 DWARF 索引 reset/init、错误码分支。
   - `tests/integration/`: 新建 `test_capture_memleak`，通过 memleak 事件 → `dwunw_capture` 的真实路径验证，确保 module cache 可重复使用。
   - `doc/`: 编写 `doc/api_usage.md`（API 调用顺序）与 `doc/cross_arch_validation.md`（交叉编译、性能、DWARF 资产），补齐 Stage 7 所需指南。
   - memleak 集成指南继续由 `examples/bpf_memleak/README.md` 承载，并在新增文档中引用。

8. **DWARF 多帧栈展开（已完成）**
> 说明：阶段 8 复用了 ghostscope/parca 的设计思路，实现了 CFI 解析+执行的最小闭环，并在 memleak 示例中保持可选开启。
   - **FDE 解析管线**：`src/dwarf/cfi.[ch]` 内实现 `.eh_frame/.debug_frame` 解析、CIE/FDE 链接、LEB128/encoded pointer 读取，并在 `dwunw_dwarf_index` 中缓存 CFI 表以便按 PC 查找。
   - **栈展开执行器**：扩展 `dwunw_capture()`，基于新的 `dwunw_cfi_eval()` 循环生成多帧；当调用者提供 `dwunw_memory_read_fn` 时即可继续展开，否则保持首帧降级。新增 `dwunw_unwind_request.read_memory`/`memory_ctx` 以让调用方插件任意地址读取逻辑。
   - **掌控回退**：若模块缺少 CFI，`dwunw_cfi_build()` 返回 `DWUNW_ERR_NO_DEBUG_DATA` 并在 capture 层静默降级；FDE 执行失败时返回对应错误码供调用方转入 FP 链或直接忽略。
   - **测试与验证**：新增 `tests/unit/test_cfi`（合成 .debug_frame、验证 CFI 执行），并更新 `test_unwinder`、`test_capture_memleak` 以覆盖新的输入要求；`Makefile` 增加 `-Isrc` 让内部头文件可复用于测试。
   - **文档更新**：`doc/api_usage.md` 补充“多帧回溯/内存读取回调”说明，强调未提供回调时的单帧退化行为及错误码含义。

## 交付里程碑
1. **M1 - Skeleton Ready**：完成阶段 1-2，产出初版库骨架与 x86_64 ops。（状态：已完成）
2. **M2 - DWARF Loader**：阶段 3 完成，可独立加载并索引 DWARF。（状态：已完成）
3. **M3 - Unwinding Core**：阶段 4 完成，具备基本回溯能力及单元测试。（状态：已完成）
4. **M4 - eBPF Integration**：阶段 5 完成，`examples/bpf_memleak/` 可跑通。（状态：已完成）
5. **M5 - Multi-arch + Tests**：阶段 6-7 完成，arm64/mips32 支持与完整测试/文档就绪。（状态：已完成）
6. **M6 - DWARF Multi-frame GA**：完成阶段 8 的全部任务，交付多帧栈展开、文档与测试。（状态：已完成）

## 风险与缓解
- **DWARF 实现复杂**：参考 `ghostscope-dwarf` 与 elfutils 文档，先实现最常用的 CFI 路径，逐步增加特性；编写小型回归样例。
- **性能超限**：在实现过程中引入性能计时代码，确保热路径无动态分配；必要时加入对象池。
- **跨架构差异**：每个架构独立单元测试，并使用 CI 交叉编译；提供 fallback 错误码让调用方可回退。
- **memleak 集成阻塞**：计划阶段 5 先以独立示例验证，之后再提交 memleak 补丁，确保主库成熟后才合并。
- **部署环境缺少 clang/llvm**：提供纯 C 的 DWARF 解析实现或预处理生成的元数据，所有发布产物仅依赖 GCC/ld 等 GNU 工具即可消费；在文档中说明如何在无 clang/llvm 的系统完成链接。

## 验证策略
- **构建验证**：CI 统一执行 `make all` 与 `make ARCH=arm64 all` 等目标，覆盖本地与交叉编译配置。
- **单元测试**：`tests/unit` 由 `ctest`/`make test` 驱动；必须输出期望/实际栈差异。
- **集成测试**：`make test` 会执行 `tests/integration/test_capture_memleak`，验证 memleak 事件转换、module cache 复用及 `dwunw_capture` 返回值；必要时再联动 `examples/bpf_memleak` 手动验证。
- **性能测试**：提供脚本在 x86_64 主机上重复回溯，确保 95% 调用 <5µs。

## 审批
- 计划状态：**阶段 8 完成，进入维护观察期**。
- 下一阶段：根据后续需求决定是否启动新的 /spec（例如 DWARF-less fallback/Perf 集成）。
