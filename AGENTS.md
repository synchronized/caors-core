<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# AGENTS.md

## 适用范围

本文件适用于仓库根目录及其所有子目录。所有自动化代理、协作者和代码生成工具在修改本项目时都应遵守这些约定。

## 项目概览

CAORS 是一个 C++20 基础设施库，目标能力包括自定义内存分配器、对象系统、运行时类型反射和跨平台序列化。项目使用 CMake 构建，并通过 `CMakePresets.json` 维护常用平台和编译器配置。

## 通用沟通要求

- 对外说明、进度更新和总结使用中文。
- 代码注释和文档使用中文。
- 回答保持简洁、直接、可执行。
- 不展示内部思考过程；需要解释时提供简要判断依据或排查摘要。

## 代码规范

- 使用 C++20 作为最低语言标准。
- 采用偏 STL/RTTR 的库风格，公共 API 使用小写下划线命名。
- 排版遵循项目 `.clang-format`，文件组织、头文件包含顺序和注释风格应与现有代码保持一致。
- 仓库文本文件统一使用 LF (`\n`) 换行；如需 CRLF，应在 `.gitattributes` 中显式声明例外。
- 优先使用标准库和 RAII 管理资源，避免不必要的裸 `new` / `delete`。
- 仅在单个 `.cpp` 文件内部使用的自由函数、变量和辅助类型应放入匿名 namespace，避免暴露翻译单元外符号。
- 公共接口应清晰表达所有权、生命周期和线程安全约束。
- 新增复杂逻辑时补充简短中文注释，避免解释显而易见的代码。

## 注释规范

- 源文件和头文件开头保留统一的 SPDX 许可证和版权声明，不添加作者、创建日期或修改记录。
- 公共头文件中的类、函数、枚举和重要类型应补充简洁的中文 Doxygen 风格注释。
- 公共 API 注释优先说明用途、参数语义、返回值、所有权、生命周期、线程安全、失败行为和重要限制。
- `.cpp` 实现文件不要求为每个函数添加注释；仅在逻辑不直观、涉及跨平台差异、内存对齐、生命周期约束或复杂算法时说明“为什么这样做”。
- 私有函数和内部细节如果可由命名和类型清晰表达，避免添加重复代码含义的注释。

## 文档规范

- 修改自有文档前阅读并遵守 [DOCUMENTATION_GUIDE.md](DOCUMENTATION_GUIDE.md)。
- 文档入口、指南、参考、概念、路线图、计划、决策和执行记录按职责维护。
- 同一事实只保留一个维护入口，其他位置使用简短摘要与相对链接。
- 当前行为、未来方案和历史证据分开；详细验收与性能数据不持续堆入计划正文。
- 新增文档接入 `docs/README.md` 对应分类或其链接的分类索引，不在根 README 维护完整清单。
- 延期事项必须明确未完成范围、原因、恢复条件和是否阻塞验收，不得记为已验证通过。

## CMake 规范

- 使用现代 target-based CMake。
- 优先在目标上声明依赖和编译选项，例如 `target_sources`、`target_include_directories`、`target_link_libraries`、`target_compile_features`。
- 不在全局范围滥用编译选项或包含目录。
- 新增模块时同步维护对应的构建目标和测试目标。
- 保持 `CMakePresets.json` 中的 preset 可用，不随意删除已有平台配置。

## 构建与测试

常用命令：

```powershell
cmake --list-presets
.\scripts\msvc.ps1 -Preset windows-msvc-debug
.\scripts\msvc.ps1 -Preset windows-msvc-shared-debug
```

Ninja + MSVC 必须在已初始化的 Visual Studio 开发环境中运行。普通 PowerShell 中应使用上述
`scripts/msvc.ps1`，不要直接调用 `windows-msvc-*` preset；脚本会通过 `vswhere` 定位 Visual
Studio 并加载 `VsDevCmd.bat`。若使用 Visual Studio generator，可在普通终端直接运行：

```powershell
cmake --list-presets
cmake --preset windows-vs2022-debug
cmake --build --preset windows-vs2022-debug
ctest --preset windows-vs2022-debug
```

Linux 示例：

```sh
cmake --preset linux-clang-debug
cmake --build --preset linux-clang-debug
ctest --preset linux-clang-debug
```

修改代码后应尽量运行与改动相关的构建和测试。若因环境缺失无法验证，需要在最终说明中明确说明未验证的原因。
构建默认不强制指定并行度；如果发生 OOM，优先降低并发。MSVC 包装脚本可使用 `-Jobs 1` 做稳定
验证；其他构建可使用 `-j 1`，或从逻辑 CPU 数量的一半以下开始并结合可用内存调整。

## 修改原则

- 保持改动聚焦，避免无关重构和格式化。
- 不回退用户已有修改，除非用户明确要求。
- 新增 API、行为变更或修复缺陷时，优先补充对应测试。
- 文档应反映当前仓库实际状态，避免声明尚未实现的具体 API。
- 引入第三方依赖前，应说明用途、替代方案和对构建的影响。

## 推荐工作流

1. 开始修改前确认当前不在 `main`。除非用户明确要求直接修改 `main`，否则从最新
   `origin/main` 创建聚焦的短期分支：
   - 新功能使用 `feature/<topic>`；
   - 缺陷修复使用 `fix/<topic>`；
   - 文档或工程维护使用 `docs/<topic>`、`chore/<topic>`。
2. 先阅读相关源码、构建文件和现有文档，明确改动边界和验证方式。
3. 在同一分支内完成相关小提交；不要为每个机械修改创建独立长期分支。
4. 实施最小必要修改，运行格式化、构建或测试等可用验证。
5. 完成一个可评审批次后提交并推送当前分支。普通分支 push 和 Pull Request 默认不触发完整 CI；
   根据改动风险完成本机相关验证。
6. 大型功能可以使用 `feature/<topic>` 作为临时集成分支，并从子分支合入；功能完整后再统一向
   `main` 创建 Pull Request。默认不建立长期 `develop` 分支。
7. 完整 Linux CI 在以下时机运行：
   - 每周一北京时间 02:00 定时运行；
   - 大功能完工、关键里程碑或高风险改动准备合并时，通过 GitHub 或
     `gh workflow run linux.yml --ref <branch>` 手动运行。
8. ABI、插件生命周期、内存管理、并发、序列化格式、构建系统或跨平台代码等高风险改动，合并前
   应主动运行完整 CI；低风险局部修改可依据本机相关测试决定是否运行。纯文档变更不触发完整 CI。
9. 未经用户明确授权，不绕过失败检查或强制合并 Pull Request。
10. 在总结中说明分支、提交、Pull Request、验证结果和剩余风险。
