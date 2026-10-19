<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# 开发规范

本文档说明 CAORS 的目录结构、代码风格、格式化、静态检查和新增模块规则。

文档的职责、目录、事实来源、计划收尾及验收记录规则见
[项目文档规范](../../DOCUMENTATION_GUIDE.md)。

## 目录结构

```text
.
├── 3rd/                    # 第三方依赖，当前包含 Catch2
├── cmake/                  # 项目 CMake 工具函数
├── docs/                   # 按职责分类的文档
├── examples/               # 可运行示例
├── include/caors/          # 公共头文件和版本头模板
├── src/                    # 库目标和实现
│   └── caors/
├── tests/                  # 测试入口
├── CMakeLists.txt
├── CMakePresets.json
└── README.md
```

## 代码规范

- 使用 C++20，不引入低于项目标准的兼容写法。
- 采用偏 STL / RTTR 的库风格：公共 API 使用小写下划线命名，例如 `get_version()`。
- 仅在单个 `.cpp` 文件内部使用的自由函数、变量和辅助类型应放入匿名 namespace，避免暴露翻译单元外符号。
- CMake 使用 target-based 写法，优先使用 `target_sources`、`target_include_directories`、`target_link_libraries` 和 `target_compile_features`。
- 公共头文件模板放在 `include/caors/`，生成头文件放在构建目录下。
- 跨库边界的公共类和非内联函数使用 `CAORS_API` 标记。
- 库实现放在 `src/caors/`。
- 版本号由 [caors_version.cmake](../../cmake/caors_version.cmake) 的 `CAORS_PROJECT_VERSION` 唯一维护，根 `project()` 读取并派生版本组件。
- 新增模块时同步补充测试目录。
- 第三方源码不默认启用项目级 `-Werror`，避免外部警告影响本项目构建。

## CMake 命名

项目私有模块统一使用 `caors_*.cmake`，辅助函数统一使用 `caors_` 前缀，避免嵌入父工程时覆盖通用名称。
CMake 约定的 `CMakeLists.txt`、`caorsConfig.cmake.in` 和生成的包文件名保持标准格式。
模块职责见 [CMake 目录说明](../../cmake/README.md)。

## API 稳定性

当前阶段与兼容政策见[API 稳定性](../reference/api-stability.md)，Beta 验收见
[执行清单](../checklists/beta-readiness.md)。本页只维护开发与公开 API 设计规则。

## 公开 API 边界

- 对外可访问的函数参数、返回值、公开数据成员和公开继承关系不暴露 `detail` 命名空间类型。
- `detail` 类型可以出现在公开头文件的私有成员、friend 声明、模板实现细节或模块内部转发函数中，但不应成为用户正常调用路径的一部分。
- 如果某个 `detail` 类型需要被用户查询，应优先增加只读 view、builder、traits 或 ops 公开包装，而不是直接移动内部结构。
- 公共测试、示例和文档应优先使用公开 API；只有专门验证内部实现边界的测试才允许包含 `detail` 头。
- 新增模块时优先让公开类型位于对应主题命名空间，例如 `caors::meta`、`caors::resources`；根命名空间只保留
  高频、跨模块、语义稳定的便捷别名。
- 不把低层策略对象、临时结果、请求状态、配置条目、注册 helper、`*_ops`、`*_fn` 或 source/provider/loader
  这类组合件放入根命名空间。新增根别名前应确认它比模块命名空间更清晰，并且不会扩大普通用户的 API 表面噪声。
- 当前允许保留在根命名空间的典型类别包括：基础错误和值结果类型、核心容器和字符串、GUID / type_id
  这类基础身份、内存资源基础 API、`meta::any` / `any_ref` / `variant` 这类跨模块值类型、对象和资源的高频语义别名、
  以及 `serialize` / `deserialize` / `to_value` / `to_string` 这类顶层操作入口。
- IO、events、tasks 的公开 API 默认只放在对应模块命名空间；对象和资源模块只把高频身份、引用和 handle
  类型放入根命名空间，manager、index、resolver、source、provider、loader、request 等组合件不放根命名空间。
- 不保留阻碍模块边界收敛的旧兼容转发头。meta value 相关代码统一使用
  `caors/meta/value*.h` 与 `caors::meta::*`。
- 类型擦除函数表使用 `*_ops` 命名，通常属于内部或高级实现；用户可见的类型能力应优先提供
  `*_capability` facade，实例访问优先返回绑定后的 `*_view`。
- 公开 capability facade 不直接暴露 `void*`。构造未知类型值时优先返回 `meta::any`，引用已有对象时优先返回
  `meta::any_ref` 或绑定后的 view；裸 `void*` 只放在底层 `*_ops`、model 和 `detail` helper 中。
- 公共头应自包含并避免依赖偶然传递 include；只需要保存指针或引用时优先前置声明，把完整内部定义留在 `.cpp` 或内部头中。

## 代码格式与静态检查

项目提供以下配置文件：

- `.clang-format`：基于 LLVM 风格微调，整体偏 STL / RTTR 这类库代码排版。
- `.clang-tidy`：启用 bugprone、performance、portability、modernize 和 readability 相关检查，并排除部分噪声较大的规则。

格式化单个文件：

```sh
clang-format -i src/caors/version.cpp
```

检查单个源文件：

```sh
clang-tidy src/caors/version.cpp -p build
```

`clang-tidy` 依赖 `compile_commands.json`，当前 preset 已开启 `CMAKE_EXPORT_COMPILE_COMMANDS`。第三方目录 `3rd/` 不作为项目代码检查目标。

`include/caors/version.h.in` 包含 CMake 占位符，不直接使用 `clang-format` 格式化；需要检查时以生成后的 `version.h` 为准。

## 开发到发布

main 是唯一集成分支，不在 main 日常开发，也不建立长期 develop。
一个完整任务复用一个短期分支和 PR；方案、实现、测试、示例和文档等子阶段在同一分支连续提交。

| 分支 | 职责 |
| --- | --- |
| `feature/<topic>` | 新功能或完整重构 |
| `fix/<topic>` | 缺陷和可移植性修复 |
| `docs/<topic>` | 文档内容与架构 |
| `chore/<topic>` | 构建、CI 和仓库维护 |
| `release/X.Y.Z` | 从最新 main 创建，只收口版本元数据、范围、变更与验收 |

执行顺序：

1. 检查工作区、当前分支、相关 Plan 与文档；新任务从最新 origin/main 创建聚焦分支，
   正在进行的同一任务继续复用原分支。
2. 明确目标、非目标与验证范围，实施最小相关改动，同步公开注释、参考、测试和示例。
3. 子阶段完成且相关验证通过后创建聚焦提交；不为每个机械步骤建立长期分支或 PR。
4. 任务达到可评审状态后推送并创建一个 PR；需要本地无法覆盖的平台或工作流验证时可提前推送同一分支。
5. 按[CI 策略](ci.md)选择验证，记录被验证 SHA、结果与未验证项；失败修复继续更新同一 PR。
6. 检查提交范围、`git diff --check`、用户可见变更与迁移说明；在授权及仓库规则范围内完成合并，
   不绕过失败检查，合并后结束短期分支。

大型功能可使用有明确出口的临时 feature 集成分支，整体完整后再向 main 提交一个 PR。
已知将进入发布的用户可见变更写入[CHANGELOG](../../CHANGELOG.md)的 Unreleased；版本范围由
[版本文档](../versions/README.md)组合关联 Plan，开发阶段不提前创建发布标签。

## 版本准备与发布的边界

功能和缺陷先经 PR 合入 main，不直接进入 release 分支。
版本准备 PR 收口工程版本、CHANGELOG、固定范围与候选验收，合入 main 后固定最终发布 SHA。
正式发布只读取该提交，执行干净验证、制品检查、tag/Release 和公开下载复核。
详细步骤统一由[发布流程](release.md)维护，检查条件由[发布清单](../checklists/release-readiness.md)维护。

当前 CAORS 没有自动发布或 Windows CI，不能照着 Granit 的脚本名运行。
发布任务必须明确目标版本和交付范围；整理流程文档本身不代表执行一次发布。
分支保护、required checks 和发布权限由仓库设置控制，本规范不声称已经自动启用这些设置。

## 测试目录规则

测试发现、目标命名、文件拆分、辅助头和特殊 CMake 入口统一见
[测试组织规范](../../tests/README.md)。

## 新增模块

新增主题模块时建议同步处理：

- 公共 API 放入 `include/caors/` 或对应主题子目录。
- 实现放入 `src/caors/`。
- 行为测试放入 `tests/<topic>/<leaf-topic>/`，遵循“叶子目录生成测试可执行文件”的规则。
- 示例放入 `examples/<topic>/`，规则见 [examples/README.md](../../examples/README.md)。
- 优先扩展对应专题文档 `docs/reference/<topic>.md`；需要独立教程、指南或概念时按文档规范选择落点，
  并在 [文档中心](../README.md) 对应分类或其链接的分类索引中挂入口。
