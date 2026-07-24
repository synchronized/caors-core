<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Tests

本目录存放 CAORS 的行为测试。测试按主题分组，并采用“叶子目录生成测试可执行文件”的组织方式。

## 运行

使用 CTest 运行当前 preset 对应测试：

```sh
ctest --preset windows-clang-debug
ctest --preset linux-gcc-debug
```

关闭测试目标：

```sh
cmake --preset windows-clang-debug -DCAORS_BUILD_TESTING=OFF
```

测试框架使用仓库内置的 Catch2，目前位于 `3rd/catch-v3.15.0/`。

## 目录与发现规则

`tests/CMakeLists.txt` 会扫描 `tests/` 下的主题目录。普通目录按叶子目录生成测试目标；如果某个目录
提供自己的 `CMakeLists.txt`，则交给该目录自行处理。

叶子目录是指没有更深测试子目录、并包含测试源文件的目录。每个叶子目录递归收集这些文件：

- `*.cpp`
- `*.hpp`
- `*.h`

每个叶子目录生成一个测试目标，目录路径会先转换为合法 C 标识符：

```text
tests/<topic>/<leaf>/ -> caors_<topic_identifier>_<leaf_identifier>_test
```

同时注册一个 CTest 用例：

```text
caors.<topic>.<leaf>
```

例如 `tests/serialization/resource/` 会生成：

```text
caors_serialization_resource_test
caors.serialization.resource
```

如果一级主题目录本身就是叶子目录，也会直接生成 `caors.<topic>`，例如 `tests/config/`。

非叶子分组目录不直接放置 `.cpp` 文件。模块共享测试辅助头文件可以放在模块根目录，例如
`tests/serialization/serialization_test_support.h`；生成测试目标时会把叶子目录和父级测试目录都加入
include 路径。

## 新增测试

新增测试时遵循以下规则：

- 同一叶子主题下的不同特性可以放到不同 `*.cpp` 文件中。
- 新主题直接新增 `tests/<topic>/`，简单主题可以把它作为叶子目录。
- 大主题优先拆成 `tests/<topic>/<leaf>/`，避免单个测试目标过大导致链接变慢。
- 测试文件使用 Catch2，优先覆盖公共 API 的行为、错误路径和边界条件。
- 普通行为测试应优先使用公开入口，例如 `meta::type`、`meta::type::get_capability<Capability>()`
  和 `any_ref::get_capability<Capability>()`。直接使用 `get_raw_ops()`、`get_registry()` extension API
  或 detail 辅助类型的测试应集中在对应的 meta/registry/static_type_record 高级主题中。
- 新增功能时优先补充对应叶子主题测试，避免把无关测试塞进已有文件。

## 头文件测试

`tests/headers/` 除了生成所有公开头文件的自包含编译测试，也可以放置手写 include 边界测试。
这类测试应使用公开 API，验证细粒度头只暴露约定能力，避免模块重新依赖隐式包含。
