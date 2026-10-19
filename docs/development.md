<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# 开发规范

本文档说明 CAORS 的目录结构、代码风格、格式化、静态检查和新增模块规则。

文档的职责、目录、事实来源、计划收尾及验收记录规则见
[项目文档规范](../DOCUMENTATION_GUIDE.md)。

## 目录结构

```text
.
├── 3rd/                    # 第三方依赖，当前包含 Catch2
├── cmake/                  # 项目 CMake 工具函数
├── docs/                   # 专题设计文档
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
- 版本号以根 `project(... VERSION ...)` 为唯一来源，不手写重复版本常量。
- 新增模块时同步补充测试目录。
- 第三方源码不默认启用项目级 `-Werror`，避免外部警告影响本项目构建。

## API 稳定性

当前阶段与兼容政策见[API 稳定性](api-stability.md)，Beta 验收见
[执行清单](checklists/beta-readiness.md)。本页只维护开发与公开 API 设计规则。

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

## 分支与 Pull Request

CAORS 使用短期功能分支和 Pull Request，不维护长期 `develop` 分支。`main` 表示已经完成评审和
验证的集成状态，不作为日常开发分支。

分支命名：

| 前缀 | 用途 | 示例 |
| --- | --- | --- |
| `feature/` | 新功能或较大重构 | `feature/meta-redesign` |
| `fix/` | 缺陷和可移植性修复 | `fix/linux-asan` |
| `docs/` | 只修改文档 | `docs/plugin-abi` |
| `chore/` | 构建、CI 和仓库维护 | `chore/development-workflow` |

推荐流程：

1. 拉取最新 `main`，创建聚焦的短期分支；
2. 在分支上完成一组相关提交，并运行与改动匹配的本地验证；
3. 功能达到可评审状态后，创建面向 `main` 的 Pull Request；
4. 高风险改动在合并前手动运行 Linux CI，修复失败项后再合并；
5. 合并后删除短期分支。

大型功能可以创建一个有明确范围的 `feature/<topic>` 临时集成分支，再把若干子分支合入该分支。
只有整体达到可评审状态时才向 `main` 创建 Pull Request。该集成分支随功能结束而删除，不演变为
长期 `develop`。

普通分支、Pull Request 和 `main` push 均不自动触发 Linux 全量构建。需要验证时，可以从 GitHub
Actions 页面或命令行手动运行 Linux workflow。同一分支连续触发时，旧的未完成运行会被自动取消，
只保留最新提交；每周定时任务继续提供环境与工具链变化的兜底检查。

当前私有仓库套餐不支持 GitHub 分支保护，仓库依靠本规范避免直接开发和推送 `main`。仓库具备
分支保护能力后，应依次启用：

- 禁止直接 push 到 `main`；
- 必须通过 Pull Request 合并；
- 对高风险改动要求手动 Linux 检查通过；
- 合并前要求分支基于最新 `main`。

## 测试目录规则

测试发现、目标命名、文件拆分、辅助头和特殊 CMake 入口统一见
[测试组织规范](../tests/README.md)。

## 新增模块

新增主题模块时建议同步处理：

- 公共 API 放入 `include/caors/` 或对应主题子目录。
- 实现放入 `src/caors/`。
- 行为测试放入 `tests/<topic>/<leaf-topic>/`，遵循“叶子目录生成测试可执行文件”的规则。
- 示例放入 `examples/<topic>/`，规则见 [examples/README.md](../examples/README.md)。
- 优先扩展对应专题文档 `docs/<topic>.md`；需要独立教程、指南或概念时按文档规范选择落点，
  并在 [文档中心](README.md) 对应分类或其链接的分类索引中挂入口。
