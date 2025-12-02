# 子模块版本矩阵

> 最近校验：2025-12-02

| 名称 | 路径 | 上游 URL | 许可证 | 固定版本/提交 | 锁定原因 | 升级步骤 | 最后校验时间 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| bcc | `src/ref/bcc` | https://github.com/iovisor/bcc | Apache-2.0 | tag `v0.32.0` (`8d85dcfac86b`) | 与 libbpf-tools/memleak 版本一致，确保示例补丁可直接复现 | 1) 建独立分支并 `git -C src/ref/bcc fetch --tags`；2) 切换目标 tag/commit 并运行 `make test`/`make examples`；3) 更新本表与 README 提示；4) 在 PR 中附 `git submodule status` 输出 | 2025-12-02 |
| libbpf | `src/ref/libbpf` | https://github.com/libbpf/libbpf | BSD-2-Clause / LGPL-2.1 | tag `v1.4.7` (`ca72d0731f8c`) | 与 memleak 示例的 headers/lib 版本匹配，避免 ABI 差异 | 同上；另外在示例构建中验证 `LIBBPF_CFLAGS/LDLIBS` 配置 | 2025-12-02 |
| ghostscope | `src/ref/ghostscope` | https://github.com/swananan/ghostscope | GPL-3.0-only | commit `8d6271f2452b` | 作为架构参考，锁定撰写规范时的最新实现 | 1) 评估新 commit 改动；2) 更新 doc/plan，若结构有变需同步引用路径；3) 重新记录 `git submodule status` | 2025-12-02 |
| parca | `src/ref/parca` | https://github.com/parca-dev/parca | Apache-2.0 | commit `279aba38f71b` | 提供 profile agent 参考，固定当前最新便于文档截图 | 同上；必要时在 examples 中确认引用片段未失效 | 2025-12-02 |

## 升级守则
- 禁止直接在 `src/ref/*` 子模块内修改代码；如需改动请先在上游仓库提交 PR，再同步新 commit。
- 升级任一子模块时必须：
  1. 在独立分支检出目标 commit/tag，并确保 `git submodule status` 输出仅包含该 commit 的更改；
  2. 更新时间戳、版本号、锁定原因等表格字段；
  3. 运行 `make test` 与 `make examples`（如依赖缺失，请在 PR 描述说明并要求 CI 覆盖）；
  4. 在 PR 描述粘贴新的 `git submodule status` 与验证命令输出。
- 如上游删除 tag/重写历史，需在 PR 中附替代来源（例如 GitHub Release tarball 的 permalink）并在本文件中注明。
