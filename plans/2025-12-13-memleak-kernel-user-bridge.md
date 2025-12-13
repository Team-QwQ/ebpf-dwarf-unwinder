# 计划：memleak 内核态异步计数与用户态 DWARF 回溯桥接

## 概述
- **范围**：落实 [specs/2025-12-11-memleak-kernel-user-bridge.md](specs/2025-12-11-memleak-kernel-user-bridge.md)，在不破坏现有 memleak 异步计数模型的前提下，新增 BPF→用户态事件管道承载可回溯上下文，并在用户态使用 `dwunw_*` 完成延迟 DWARF 回溯。
- **目标**：定义并实现可移植的异步事件格式、内核采样最小补丁、用户态回溯与缓存路径，以及文档/测试验证，使 glibc `-fomit-frame-pointer` 场景下 ≥90% 泄漏热点可输出 ≥5 层调用栈，其余标记 `DWUNW_PARTIAL`。

## 假设与约束
- 仅调整 `examples/bpf_memleak/` 与 `examples/memleak_bcc_dwunw/`，不改动 `src/ref/*`；默认以 x86_64 为首发架构，arm64/mips32 作为兼容性检查即可。
- 用户态具备 `CAP_SYS_PTRACE`；attach 失败时需向 CLI 显式暴露错误并回退到 caller 地址输出。
- BPF 热路径不可阻塞；事件结构需 8 字节对齐，允许 `BPF_RINGBUF_DISCARD` 丢弃并统计丢包。
- 仅依赖现有 `make examples`/`make test` 流程，无新增构建系统。

## 分阶段计划
1. **阶段A：事件结构与采样基线**
   - 在两套示例中定义 `struct dwunw_async_event`（tgid/pid/tid、alloc_id、timestamp、ip/sp、可选 stackid 或固定长度 stack slice、寄存器子集、flags），确保对齐与大小常量归档至共享头文件。
   - 内核侧采样：基于 tracepoint/kprobe 路径收集 IP、SP、BP/FP、LR，若 BPF helper 支持则捕获用户栈 stackid；如不支持直接读取用户栈，则留空由用户态补采。
   - ring buffer 输出策略：默认 `BPF_RINGBUF_OUTPUT` 成功路径，失败/丢包计入统计 map。

2. **阶段B：事件过滤与触发控制**
   - 增加触发/白名单 map（按 PID/stack hash/alloc_id）或“未释放阈值”逻辑，BPF 仅在命中时推送事件。
   - 用户态 CLI 支持配置触发策略（阈值、白名单文件或参数），并通过 `bpf_map_update_elem` 下发；提供默认防洪参数。
   - 在 README 说明丢包/过滤字段及调优建议。

3. **阶段C：用户态回溯管线落地**
   - 在 memleak 用户态事件消费路径中：
     - 依据事件 `pid/tid` 调用 `dwunw_stack_reader_attach()/read()`，填充真实栈内容/寄存器；attach 失败记录错误码并沿用原 caller 地址。
     - 调用 `dwunw_regset_prepare()` + `dwunw_capture()` 生成帧数组，将 `DWUNW_FRAME_FLAG_PARTIAL` 传播到输出。
   - 输出聚合：以 `stack_hash` 为键缓存 `dwunw_frame[]`，命中直接复用；模块重载或错误码时刷新缓存。
   - CLI 增加模式切换（force/off/auto）及事件调试开关，保持原有输出格式的向后兼容性。

4. **阶段D：统计、降级与兼容处理**
   - 记录 ring buffer 丢包数、attach 失败、栈读取失败、回溯失败等指标并在 CLI 周期性打印；必要时提供 JSON/文本输出选项。
   - 设计降级顺序：事件缺失 stack/regs → 尝试用户态补栈 → 仍失败则输出 caller + `DWUNW_PARTIAL`。所有降级分支需在文档与 README 描述。
   - 确认 event/capture 路径与现有 memleak 聚合数据结构兼容，不破坏历史命令行参数。

5. **阶段E：文档与验收**
   - 更新 `doc/api_usage.md`、`doc/dwunw_design.md`、`examples/bpf_memleak/README.md`/`examples/memleak_bcc_dwunw/README.md`，描述事件格式、权限需求、过滤策略、降级标记与验证步骤。
   - 测试计划：
     - `make test` 通过现有单元/集成测试，必要时扩充 `tests/integration/test_capture_memleak.c` 验证新事件字段与回溯路径。
     - 手工场景：以 `DWUNW_TEST_FIXTURE` 运行 memleak 示例，验证 `--mode force/off/auto`、过滤策略及丢包统计；记录示例命令。
   - 验收标准对齐 spec：验证 ≥90% 热点 ≥5 层栈、ring buffer 丢包率 <1%、热路径新增指令评估（perf stat）。

## 影响与依赖
- 主要文件：`examples/bpf_memleak/*`、`examples/memleak_bcc_dwunw/*`、`doc/api_usage.md`、`doc/dwunw_design.md`、各 README。
- 依赖：libbpf + CAP_SYS_PTRACE 环境；现有 `dwunw_stack_reader`、`dwunw_capture` API。

## 风险与缓解
- **ptrace 不可用或 attach 慢**：快速失败并标记 `DWUNW_PARTIAL`；CLI 提示权限缺失；可配置禁用回溯走纯 caller 路径。
- **ring buffer 洪泛/采样过重**：默认开启过滤/阈值，提供丢包计数与调优参数；必要时缩短 stack slice。
- **栈快照不足导致回溯失败**：事件包含 stackid 以便用户态二次读取，或在 CLI 提示增加 slice；缓存成功帧减少重复读取。
- **兼容性破坏**：保留旧输出字段并在 README 标注新增字段/模式；对 BPF 对齐/大小变化进行 verifier 检查。

## 验证策略
- 运行 `make test`；更新/新增集成测试覆盖新事件字段、attach 失败降级与缓存命中路径。
- 手工验证：以示例 BPF 程序在本地生成泄漏事件，测试 force/off/auto 模式、过滤参数、丢包统计；使用 `perf stat` 评估 BPF 热路径开销。
- 结果记录：在 README 中列出验证命令与期望输出片段，必要时附带 perf/丢包统计示例。

## 审批状态
- 当前阶段：`/plan`
- 下一步：获得确认后进入 `/do` 实施。
