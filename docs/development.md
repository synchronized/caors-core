<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# 开发规范

本文档说明 CAORS 的目录结构、代码风格、格式化、静态检查和新增模块规则。

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

CAORS 当前仍处于早期设计和验证阶段，暂不承诺公共 API 稳定性。修改公共接口时优先保证模块边界清晰、
命名一致、所有权和生命周期语义正确；如果旧接口阻碍这些目标，可以直接做破坏性调整，并同步更新源码、测试、
示例和文档。进入稳定版本前再重新定义弃用周期、兼容层和迁移策略。

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

## 测试目录规则

`tests/` 采用“叶子目录生成测试可执行文件”的组织方式：

- 每个叶子测试目录自动生成一个测试目标和一个 CTest 用例。例如
  `tests/serialization/resource/` 会生成 `caors_serialization_resource_test`，CTest 名称为
  `caors.serialization.resource`。
- 非叶子分组目录不直接放置 `.cpp` 文件；如果一个目录还有子目录，测试源文件应移动到更具体的叶子目录。
- 叶子目录可以包含多个同主题测试源文件，但不采用“一个用例一个超长 `.cpp` 文件”的规则。
- 测试源文件按小主题命名，例如 `vector_test.cpp`、`registration_test.cpp`、`object_ref_test.cpp`；
  详细场景写在 `TEST_CASE` 名称中，不重复写进文件名。
- 文件名不重复父目录已有信息。例如位于 `tests/serialization/container/` 下时，文件名不再添加
  `serialization_container_` 前缀。
- 单个主题文件可以包含多个相关 `TEST_CASE`；当文件明显过大或 helper 名称冲突时，再拆成更具体的短主题文件。
- 测试文件名应保持简短，避免 Windows + CMake + Ninja 生成对象文件路径时触发路径长度限制。
- 模块共享的测试辅助头文件可以放在模块根目录，例如 `tests/serialization/serialization_test_support.h`；
  测试目标会包含叶子目录及其父级测试目录。
- 需要特殊构建规则的测试目录可以提供自己的 `CMakeLists.txt`，根测试脚本会改用
  `add_subdirectory()` 交给该目录自行处理。
- 新增测试时优先新增或复用合适的叶子主题目录，避免把大量不相关测试继续塞进单个大目标。

## 新增模块

新增主题模块时建议同步处理：

- 公共 API 放入 `include/caors/` 或对应主题子目录。
- 实现放入 `src/caors/`。
- 行为测试放入 `tests/<topic>/<leaf-topic>/`，遵循“叶子目录生成测试可执行文件”的规则。
- 示例放入 `examples/<topic>/`，规则见 [examples/README.md](../examples/README.md)。
- 专题文档放入 `docs/<topic>.md`，并在 [docs/README.md](README.md) 中挂入口。
