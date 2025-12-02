# 引用子模块版本锁定规范

## 背景
- `src/ref/` 下的四个子模块当前跟踪不同 upstream 分支/标签，缺乏统一的版本约束，导致在不同开发者或 CI 机器上执行 `git submodule update --init --recursive` 时会因为上游 repo 发生 fast-forward 而得到不同的依赖版本。
- 为保持与 `libbpf-tools/memleak` 的参考实现兼容，`bcc` 与 `libbpf` 需要分别固定在 `v0.32.0` 与 `v1.4.7`，否则示例补丁和 API 行为会出现不可预期的偏差。
- 其余 `ghostscope`、`parca` 两个仓库主要作为架构参考，只需锁定当前最新提交即可，后续若需升级再行评估。

## 目标
1. 将 `src/ref/bcc` 固定在 tag `v0.32.0`（commit `8d85dcfac86bb7402a20bea5ceba373e5e019b6c`）。
2. 将 `src/ref/libbpf` 固定在 tag `v1.4.7`（commit `ca72d0731f8c693bd98caba70d951fc0bfe20788`）。
3. 将 `src/ref/ghostscope` 固定在当前最新提交 `8d6271f2452b22e29d0fbe8701879308d585e6d7`。
4. 将 `src/ref/parca` 固定在当前最新提交 `279aba38f71b448972b6eeba1254ccfdfc16441f`。
5. 在 `doc/` 目录新增一份“子模块版本矩阵”文档，列出上述四个仓库的来源、协议、固定版本、更新时间以及升级流程，确保未来升级时有据可查。
6. 在 README 或相关构建文档中加入一句提示：更新子模块前先确认 `doc/submodule_versions.md` 中的受控版本，避免误升。

## 非目标
- 不引入新的子模块，也不移除现有子模块。
- 不改变 `Makefile`、构建脚本或示例代码的依赖拓扑，仅锁定版本与文档。
- 不实现自动化的子模块同步脚本；升级流程依旧走手动审核。

## 详细需求
### 版本锁定
- 父仓库应记录上述 commit hash，`git submodule status` 输出需与“目标”章节一致。
- 若上游仓库缺少对应 tag（例如未来新增仓库），需在“版本矩阵”中注明采用的 commit 及获取方式。

### 文档更新
- 在 `doc/submodule_versions.md`（新文件）中建立表格：字段包含 `名称`、`路径`、`上游 URL`、`许可证`、`固定版本/提交`、`锁定原因`、`升级步骤`、`最后校验时间`。
- 在 `README.md` 的开发者指南或克隆步骤中增加一句：`git submodule update --init --recursive` 之后需对照上述文档确认版本。

### 验证
- 执行 `git submodule status`，确保输出行首状态为 ` `（空格）且 commit 与目标一致。
- 运行 `make examples` 与 `make test`（至少在 x86_64 环境）以验证新版本不会破坏当前示例与单元测试。

## 验收标准
- 版本矩阵文档存在且覆盖四个子模块，信息完整。
- README 中新增提示文字且语言与现有文档风格一致。
- 本仓库 HEAD 指向的四个子模块提交与“目标”章节完全一致，CI 能在 clean clone + `git submodule update --init --recursive` 环境下复现相同版本。

## 风险与缓解
- **上游重写历史或删除 tag**：在版本矩阵中补充“升级步骤”并要求引用 GitHub release tarball 的 permalink 以便 fallback。
- **误操作导致子模块进入分离 HEAD 状态**：在 README 中强调禁止直接对 `src/ref/*` 提交本地变更，需通过 upstream PR 或自有 fork。
- **未来需要升级依赖**：通过文档化流程，要求更新人先在独立分支验证 `make test` 与 `make examples` 后再提交。
