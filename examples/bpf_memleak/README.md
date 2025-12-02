# memleak eBPF 示例（阶段 5/8）

本目录提供与 `libdwunw` 集成的最小 memleak 工作流示例，包含以下组件：

- `memleak_bpf.c`：基于 CO-RE 的 kprobe 程序，在 `do_exit` 上采集 `pt_regs` 并通过 ring buffer 投递事件。
- `memleak_events.h`：BPF 与用户态共享的事件定义，兼容 `dwunw_regset` 转换。
- `memleak_user.c`：用户态加载器，使用 libbpf 创建 ring buffer 订阅并调用 `dwunw_capture` 生成栈帧。

> ⚠️ 该示例用于演示 Stage 5 的基础数据通路以及 Stage 8 引入的多帧 DWARF 展开。实际 memleak 集成仍需根据生产宿主机、符号源以及 uprobe 点位进行细化。

## 先决条件

1. 可用的 libbpf（>= 1.1），并提供 `pkg-config` 条目或在环境变量中设置 `LIBBPF_CFLAGS`/`LIBBPF_LDLIBS`。
2. clang/llvm（仅用于编译 eBPF `.bpf.o`，运行时及 `libdwunw.a` 链接不依赖 clang）。
3. `bpftool`：用于生成 `vmlinux.h`（如果系统尚未提供）以及检查 BTF。
4. root 权限或 CAP_BPF/CAP_SYS_ADMIN，以便加载 kprobe 程序。

## 构建步骤

1. **准备 vmlinux.h（若系统未提供）**

   ```bash
   bpftool btf dump file /sys/kernel/btf/vmlinux format c > examples/bpf_memleak/vmlinux.h
   ```

2. **编译 eBPF 对象**

   ```bash
   clang -g -O2 -target bpf \
     -D__TARGET_ARCH_x86 \
     -Iexamples/bpf_memleak \
     -I/usr/include \
     -c examples/bpf_memleak/memleak_bpf.c \
     -o examples/bpf_memleak/memleak_bpf.o
   ```

   - `-D__TARGET_ARCH_x86` 仅用于示例，可按需替换为其他架构。
   - 如果使用 CO-RE，可追加 `-D__BPF_TRACING__` 等自定义宏。

3. **构建用户态示例**

   ```bash
   make ARCH=$(uname -m) examples \
     LIBBPF_CFLAGS="$(pkg-config --cflags libbpf)" \
     LIBBPF_LDLIBS="$(pkg-config --libs libbpf)"
   ```

   产物位于 `build/$(uname -m)/examples/bpf_memleak/memleak_user`。

## 运行示例

1. 启动用户态加载器（需 root 权限）

   ```bash
   sudo build/$(uname -m)/examples/bpf_memleak/memleak_user \
     --bpf ./examples/bpf_memleak/memleak_bpf.o \
     --symbol do_exit
   ```

   可选参数：
   - `--duration <sec>`：运行时长，默认 10s。
   - `--quiet`：禁用每次事件的详细打印。

   > 多帧 DWARF 展开依赖 `/proc/<pid>/mem` 读取寄存器之上的调用栈槽。如果不想全程以 root 运行，可在构建产物上授予 `CAP_SYS_PTRACE`：
   > ```bash
   > sudo setcap cap_sys_ptrace=+ep build/$(uname -m)/examples/bpf_memleak/memleak_user
   > ```
   > 若权限不足，示例会自动回退到单帧输出并打印告警。

2. 触发一些进程退出（如 `sleep 1`），即可看到示例输出：

   ```text
   [event] pid=1234 comm=sleep pc=0xffffffff810a0c30
     #0 pc=0xffffffff810a0c30 sp=0xffffb18f00307d60 ra=0xffffffff810a0c30 flags=partial
   ```

3. 终止程序：Ctrl+C 或等待 `--duration` 结束。`memleak_user` 会调用 `dwunw_shutdown` 清理模块缓存。

## Stage 8：多帧栈展开要点

1. `memleak_user.c` 在接收到事件时会尝试打开 `/proc/<pid>/mem` 并注册 `read_memory` 回调，使 `dwunw_capture()` 能够沿 DWARF CFI 继续向上解析父帧。
2. 如果进程不允许读取其 `mem` 文件（常见于容器或未授予 `CAP_SYS_PTRACE` 的环境），示例会打印 `[warn] open /proc/<pid>/mem failed` 并自动回退到单帧模式，保持与 Stage 5 行为兼容。
3. 出于安全考虑，示例不会缓存文件描述符；每次事件结束都会主动关闭 `mem`，避免长时间持有高权限句柄。
4. 需要验证多帧路径时，可借助 `tests/integration/test_capture_memleak` 或直接观察 `frames` 数组是否超过 1 条记录。

## 关键代码路径

- **事件结构**：`memleak_events.h`，同时服务于 BPF（`__BPF__`）和用户态：
  - BPF 侧只依赖内核类型；
  - 用户态侧提供 `memleak_event_to_regset()`，直接填充 `dwunw_regset`。
- **BPF 程序**：`memleak_bpf.c` 在 `do_exit` kprobe 中记录 `pt_regs` 并推送 ring buffer。
- **用户态 loader**：`memleak_user.c` 使用 `libbpf` 打开 `.bpf.o`，附加至 `do_exit`，并将事件转化为 `dwunw_capture` 所需输入。

## 后续扩展建议

1. **真实 memleak 集成**：
   - 将 `kprobe/do_exit` 替换为 memleak 现有的 uprobe/tracepoint；
   - `cookie` 字段可与 memleak 的对象缓存对齐，用于关联分配信息。
2. **多架构支持**：
   - 扩展 `memleak_capture_regs()` 对 arm64/mips32 的寄存器布局；
   - 在用户态根据 `evt->arch` 决定符号解析策略。
3. **调试符号来源**：
   - 使用 debuginfod 或提前生成 `.debug` 文件，传给 `dwunw_module_cache`。
4. **错误处理/回退**：
   - 当前示例若 DWARF 缺失会返回 `DWUNW_ERR_NO_DEBUG_DATA`；可拓展为回退到 FP unwinder 或跳过该样本。

## 清理

```bash
rm -f examples/bpf_memleak/memleak_bpf.o
make clean
```

完成 Stage 8 后，后续阶段继续聚焦跨架构支持与完整测试矩阵（阶段 6-7 已完成，当前分支示例已具备多帧能力）。
