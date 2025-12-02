# 计划：引用子模块版本锁定

## 概述
- **范围**：落实 `specs/2025-12-02-submodule-version-pinning.md` 描述的版本固定策略，确保 `src/ref/bcc`、`src/ref/libbpf`、`src/ref/ghostscope`、`src/ref/parca` 在仓库内始终指向受控的 commit，并补充配套文档提醒。
- **目标**：提供可核验的子模块版本矩阵、README 提示以及回归验证步骤，使 CI 与开发者在任意时间同步子模块后得到一致依赖。

## 关联规范
- 主规范：`specs/2025-12-02-submodule-version-pinning.md`

## 假设与非目标
- 假设：网络可访问四个上游仓库，`git` 支持以 commit/tag 方式检出；当前 `git submodule status` 已在所有子模块处于 detached HEAD 状态。
- 非目标：不新增/删除子模块、不修改 Makefile、构建脚本或示例代码的依赖，仅调整子模块指向与文档。

## 分阶段计划
1. **阶段A：锁定提交**
   - 使用 `git submodule update --init --recursive` 确认所有子模块已下载。
   - `git -C <path> checkout <commit-or-tag>`：
     - `src/ref/bcc` → tag `v0.32.0` (`8d85dcfac86bb7402a20bea5ceba373e5e019b6c`)
     - `src/ref/libbpf` → tag `v1.4.7` (`ca72d0731f8c693bd98caba70d951fc0bfe20788`)
     - `src/ref/ghostscope` → commit `8d6271f2452b22e29d0fbe8701879308d585e6d7`
     - `src/ref/parca` → commit `279aba38f71b448972b6eeba1254ccfdfc16441f`
   - 执行 `git submodule status` 确认四行均以空格前缀显示所需 hash，并记录在 plan 检查表中。

2. **阶段B：文档矩阵**
   - 新建 `doc/submodule_versions.md`，采用表格列出：名称、路径、上游 URL、许可证、固定版本/提交、锁定原因、升级步骤（含验证要求）、最后校验时间。
   - 描述升级流程：提交者需更新表格、重新运行验证命令，并在 PR 说明中贴出新的 `git submodule status`。

3. **阶段C：README 提示**
   - 在 `README.md` 的“克隆/构建前置”或“开发者指南”章节追加一句：执行 `git submodule update --init --recursive` 后，需对照 `doc/submodule_versions.md` 确认版本，禁止擅自升级。
   - 若 README 已有相关段落，插入提示确保风格一致（中文，简洁）。

4. **阶段D：验证与记录**
   - 再次运行 `git submodule status`，截图（或复制输出）至 PR 描述，确保与阶段A一致。
   - 在 x86_64 环境执行 `make test` 与 `make examples`，确认版本锁定未破坏现有构建；若环境受限，可记录无法执行的原因并在计划中标记需后续补跑。
   - 检查 `doc/submodule_versions.md`、`README.md` 有无错别字或链接失效。

## 影响与依赖
- 影响文件：`.gitmodules`（如需添加 `branch =` 指定；若不变则只影响子模块 HEAD）、`doc/submodule_versions.md`、`README.md`。
- 依赖：访问 GitHub 上游的网络；本地需安装 GNU Make 以完成验证。

## 风险与缓解
- **风险：上游删除/重写 tag** → 在文档中记录获取方式与备用 URL（如 release tarball），并建议在升级前保存当前 tarball。
- **风险：开发者误 checkout 非受控分支** → README 与 doc 强调“不得在 `src/ref/*` 内直接开发”，如需修改必须在 upstream 处理。
- **风险：验证无法执行** → 若本地缺少依赖，需在 PR 中说明并请求 CI 补跑；计划中要求至少在一个环境上成功运行 `make test`、`make examples`。

## 验证策略
- 命令：`git submodule status`、`make test`、`make examples`。
- 结果记录：在 PR 说明附上命令输出；若 `make examples` 依赖特殊库（如 libbpf headers），需在文档写明如何配置 `LIBBPF_CFLAGS/LDLIBS`。

## 审批状态
- 当前阶段：`/plan`
- 下一步：待评审通过后进入 `/do`。
