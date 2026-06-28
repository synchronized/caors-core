<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Tests

本目录存放 CAORS 的行为测试。测试按主题组织，每个非空主题目录会生成一个独立测试可执行文件。

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

`tests/CMakeLists.txt` 会扫描 `tests/` 下的一级主题目录，并递归收集这些文件：

- `*.cpp`
- `*.hpp`
- `*.h`

如果主题目录中没有测试源码，该目录会被忽略，不生成测试目标。

每个非空主题目录生成一个测试目标，目录名会先转换为合法 C 标识符：

```text
tests/<topic>/ -> caors_<topic_identifier>_test
```

同时注册一个 CTest 用例：

```text
caors.<topic>
```

## 新增测试

新增测试时遵循以下规则：

- 同一主题下的不同特性可以放到不同 `*.cpp` 文件中。
- 新主题直接新增 `tests/<topic>/` 目录并放入测试源码。
- 测试文件使用 Catch2，优先覆盖公共 API 的行为、错误路径和边界条件。
- 新增功能时优先补充对应主题测试，避免把无关测试塞进已有文件。
