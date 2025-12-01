# 计划：DWARF eBPF 栈回溯库

## 概述
- **范围**：实现 `specs/2025-12-01-dwarf-ebpf-stack-unwinder.md` 中定义的静态库、示例与文档，重点支持 `libbpf-tools/memleak` 的 eBPF 场景。
- **目标**：在不修改内核的前提下，提供跨架构、可插拔的 DWARF 栈回溯实现，并给出与 memleak 集成的可验证示例。
- **交付物**：`libdwunw.a`、API/架构文档、`examples/bpf_memleak/` 示例、针对 x86_64/arm64/mips32 的测试资产。

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

3. **DWARF/ELF 装载与索引**
   - 实现以文件路径为唯一入口的 ELF 载入器；后续若需要支持 fd/内存缓冲，将另行扩展接口。
   - 编写 DWARF 索引模块：解析 `.debug_info/.debug_frame/.eh_frame`，构建可重用的 CIE/FDE 索引；支持懒加载与缓存。
   - 设计符号和模块缓存，支持多模块并行加载（对标 `ghostscope` 模型）。

4. **栈回溯引擎**
   - 实现 CFA/RA 解算器：利用 DWARF CFI 解析每一帧的寄存器恢复和返回地址。
   - 将架构 ops、DWARF 索引与调用方提供的帧缓冲串联，完成 `dwunw_capture` 主循环。
   - 增加错误处理路径：DWARF 缺失、寄存器不足、地址越界等，输出细粒度错误码。

5. **eBPF 事件集成 & 示例**
   - 在 `examples/bpf_memleak/` 下创建用户态代码：读取 BPF ring buffer 事件、构造 `dwunw_regs`、调用 `dwunw_capture`。
   - 给出 BPF 程序补丁：在 memleak 的 uprobe 中记录寄存器、SP、模块 cookie 并输出到 ring buffer。
   - 提供 README（中文）说明如何构建、加载、运行示例以及如何验证输出。

6. **跨架构支持扩展**
   - 基于阶段 4 的框架补全 arm64/mips32 ops，实现寄存器映射与 CFI 适配。
   - 使用预制的 ELF 样例或交叉编译测试二进制，验证 `dwunw_capture` 能产生正确帧序列。

7. **测试与文档完善**
   - `tests/unit/`: 覆盖 DWARF 解析、CFA 计算、错误码；使用模拟寄存器输入。
   - `tests/integration/`: 利用真实 ELF 和录制的 BPF 事件，比较预期/实际栈。
   - `doc/`: 补充 API 指南、内存与性能注意事项、交叉编译说明。
   - 准备 memleak 集成指南：如何启用/禁用 `libdwunw`、回退策略等。

## 交付里程碑
1. **M1 - Skeleton Ready**：完成阶段 1-2，产出初版库骨架与 x86_64 ops。（状态：已完成，GNU Make 构建、目录骨架与 PAL 就绪）
2. **M2 - DWARF Loader**：阶段 3 完成，可独立加载并索引 DWARF。
3. **M3 - Unwinding Core**：阶段 4 完成，具备基本回溯能力及单元测试。
4. **M4 - eBPF Integration**：阶段 5 完成，`examples/bpf_memleak/` 可跑通。
5. **M5 - Multi-arch + Tests**：阶段 6-7 完成，arm64/mips32 支持与完整测试/文档就绪。

## 风险与缓解
- **DWARF 实现复杂**：参考 `ghostscope-dwarf` 与 elfutils 文档，先实现最常用的 CFI 路径，逐步增加特性；编写小型回归样例。
- **性能超限**：在实现过程中引入性能计时代码，确保热路径无动态分配；必要时加入对象池。
- **跨架构差异**：每个架构独立单元测试，并使用 CI 交叉编译；提供 fallback 错误码让调用方可回退。
- **memleak 集成阻塞**：计划阶段 5 先以独立示例验证，之后再提交 memleak 补丁，确保主库成熟后才合并。
- **部署环境缺少 clang/llvm**：提供纯 C 的 DWARF 解析实现或预处理生成的元数据，所有发布产物仅依赖 GCC/ld 等 GNU 工具即可消费；在文档中说明如何在无 clang/llvm 的系统完成链接。

## 验证策略
- **构建验证**：CI 统一执行 `make all` 与 `make ARCH=arm64 all` 等目标，覆盖本地与交叉编译配置。
- **单元测试**：`tests/unit` 由 `ctest`/`make test` 驱动；必须输出期望/实际栈差异。
- **集成测试**：运行 `examples/bpf_memleak/run.sh`，对比 frame pointer vs DWARF 栈；日志需包含成功率统计。
- **性能测试**：提供脚本在 x86_64 主机上重复回溯，确保 95% 调用 <5µs。

## 审批
- 计划状态：**执行中（阶段 2 完成）**。
- 下一阶段：继续 `/do`，推进阶段 3（DWARF/ELF 装载与索引）。
