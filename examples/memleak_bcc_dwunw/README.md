# memleak_bcc_dwunw 示例

> 基于 `src/ref/bcc/libbpf-tools/memleak*`（commit `8d85dcfac86bb7402a20bea5ceba373e5e019b6c`, 即 v0.32.0）拷贝，并在必要位置加入 `libdwunw` 适配。所有新增/改动均使用 `dwunw-added` 注释标明，可通过以下命令快速查看差异：
>
> ```bash
> diff -u src/ref/bcc/libbpf-tools/memleak.c examples/memleak_bcc_dwunw/memleak_dwunw_user.c | grep dwunw-added
> diff -u src/ref/bcc/libbpf-tools/memleak.bpf.c examples/memleak_bcc_dwunw/memleak_dwunw.bpf.c | grep dwunw-added
> ```

## 目录结构

- `memleak_dwunw.bpf.c`：源自 upstream `memleak.bpf.c`，新增 `dwunw_events` ring buffer，记录 uprobes 入口时的寄存器快照。
- `memleak_dwunw_user.c`：源自 upstream `memleak.c`，新增 `--dwunw-mode` CLI，创建 `dwunw_context` 并消费 ring buffer。
- `memleak_dwunw_events.h`：BPF/用户态共享的寄存器快照定义，避免直接在 BPF 侧包含 `dwunw` 头文件。
- `trace_helpers.*`、`maps.bpf.h`、`core_fixes.bpf.h`、`vmlinux.h`：与 upstream 保持一致，用于最小可用示例。

## 构建

1. 构建并安装 repo 自带的 libbpf（v1.4.7）到本地 `build/libbpf` 目录：
   ```bash
   make -C src/ref/libbpf/src BUILD_STATIC_ONLY=1 \
     DESTDIR=$PWD/build/libbpf prefix=/usr install
   ```
   完成后可获得 `build/libbpf/usr/include` 与 `build/libbpf/usr/lib64/libbpf.a`，供示例静态链接。
2. 确保存在 `bpftool`（例如 `sudo apt install linux-tools-common linux-tools-$(uname -r)` 后使用 `/usr/sbin/bpftool`）。
3. 编译示例：
   ```bash
   make ARCH=$(uname -m) examples \
     BPFTOOL=/usr/sbin/bpftool \
     LIBBPF_CFLAGS="-I$PWD/build/libbpf/usr/include" \
     LIBBPF_LDLIBS="-L$PWD/build/libbpf/usr/lib64 -lbpf -lelf -lz"
   ```
   生成物：
   - `build/$(uname -m)/examples/memleak_bcc_dwunw/memleak_dwunw.bpf.o`
   - `build/$(uname -m)/examples/memleak_bcc_dwunw/memleak_dwunw.skel.h`
   - `build/$(uname -m)/examples/memleak_bcc_dwunw/memleak_dwunw_user`

## 运行

```bash
sudo build/$(uname -m)/examples/memleak_bcc_dwunw/memleak_dwunw_user \
  --dwunw-mode=fallback \
  -p $(pidof target) -O libc.so.6
```

- `--dwunw-mode=force`：只输出由 `libdwunw` 解析的栈；若默认 helper 在 `ptrace`/`process_vm_readv` 阶段失败（缺少 `CAP_SYS_PTRACE` 或进程设置了 Yama 限制）会直接报错；
- `--dwunw-mode=fallback`（默认）：`dwunw_capture` 失败时回退到原有 `ksyms`/`syms_cache` 逻辑；
- `--dwunw-mode=off`：完全关闭 `dwunw`，与 upstream 行为一致。

推荐在可执行文件上授予 `CAP_SYS_PTRACE`，以便无需 root 也能读取 `/proc/<pid>/mem`：

```bash
sudo setcap cap_sys_ptrace=+ep build/$(uname -m)/examples/memleak_bcc_dwunw/memleak_dwunw_user
```

当运行在 `fallback` 模式且默认 reader 失败或 `dwunw_capture` 报错时，程序会打印 `[dwunw] default reader failed ... retry without reader` 日志并重新发起单帧捕获，保证与 BCC 原版逻辑保持一致。

Ring buffer 输出示例：

```
[dwunw] pid=1234 comm=python frames=2
  [dwunw] #0 pc=0x7f... sp=0x7ffc... ra=0x0 flags=0 module=/usr/bin/python3.11
```

## Stage 8 增强内容

1. `memleak_dwunw_user.c` 通过在 `dwunw_unwind_request` 中填充 `pid/tid`，激活库内默认 reader（`ptrace + process_vm_readv + /proc/<pid>/mem`），可一次返回 8 帧以内的完整调用栈。
2. `--dwunw-mode=fallback` 会在默认 reader 失败时自动回退至首帧输出；`force` 模式则保持“失败即报错”语义，便于在生产中快速发现权限问题。
3. 日志中新增 `multi-frame capture failed` 与 `multi-frame disabled` 两类提示，帮助区分 reader 权限不足与 DWARF 缺失两种异常。

## 验证步骤

1. `make examples`：同时构建 `examples/bpf_memleak/memleak_user` 与本目录的 `memleak_dwunw_user`；
2. `make test`：确保 core/unwinder 单元测试与示例共存；
3. 运行 `--dwunw-mode=off` 验证原有输出，运行 `--dwunw-mode=force` 验证 DWARF 栈；
4. 通过 `grep -R "dwunw-added" examples/memleak_bcc_dwunw` 确认所有新增代码均有标记。

## 备注

- 若重新安装 libbpf 或清理 `build/` 目录，需重复执行“构建 libbpf”步骤再运行 `make examples`，否则链接阶段会找不到 `-lbpf`。
- 示例默认针对 x86_64 的 `struct pt_regs`，其他架构可参考 `memleak_dwunw_events.h` 增加寄存器布局并在 BPF 程序中填充。
- 真实环境应改写为使用 debuginfod 或进程专用 ELF 路径；本示例简单使用 `/proc/<pid>/exe`。
- 由于 skeleton 在 `build/.../memleak_dwunw.skel.h` 下生成，请勿将该文件加入版本控制；需要重新编译 BPF 时，执行 `make clean` 即可。
