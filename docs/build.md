<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# 构建与安装

本文档说明 CAORS 的环境要求、常用构建命令、测试入口、CMake 选项和安装方式。

## 环境要求

- CMake 3.23 或更新版本。
- Ninja。
- 支持 C++20 的编译器，任选其一：
  - MSVC `cl`
  - `clang-cl`
  - Clang
  - GCC

在 Windows 使用 MSVC preset 时，需要从 Visual Studio Developer PowerShell 或已配置 `cl` 的终端运行命令。

## 快速开始

查看可用 preset：

```sh
cmake --list-presets
```

配置、构建并测试：

```sh
cmake --preset windows-clang-debug
cmake --build --preset windows-clang-debug
ctest --preset windows-clang-debug
```

使用 GCC preset：

```sh
cmake --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug
ctest --preset linux-gcc-debug
```

preset 名称表示预期工具链组合，实际是否可用取决于当前系统和 PATH 中的编译器。

## 常用 Preset

| Preset | 用途 |
| --- | --- |
| `windows-msvc-debug` | Windows MSVC Debug 构建 |
| `windows-msvc-release` | Windows MSVC Release 构建 |
| `windows-clang-cl-debug` | Windows clang-cl Debug 构建 |
| `windows-clang-debug` | Clang Debug 构建 |
| `linux-clang-debug` | Clang Debug 构建 |
| `linux-gcc-debug` | GCC Debug 构建 |

当前所有 preset 共用默认构建目录 `build/`。在不同编译器 preset 之间切换时，如果 CMake 提示编译器缓存变化，建议重新配置或清理 `build/` 后再构建。

## 测试

测试说明见 [tests/README.md](../tests/README.md)。这里仅保留常用入口：

```sh
ctest --preset windows-clang-debug
ctest --preset linux-gcc-debug
```

## CMake 选项

| 选项 | 默认值 | 说明 |
| --- | --- | --- |
| `BUILD_SHARED_LIBS` | `OFF` | 是否将 `caors` 构建为共享库 |
| `CAORS_BUILD_TESTING` | `ON` | 是否构建测试目标 |
| `CAORS_BUILD_EXAMPLES` | 顶层项目为 `ON` | 是否构建示例目标 |
| `CAORS_BUILD_BENCHMARKS` | `OFF` | 是否构建 benchmark 目标 |
| `CAORS_ENABLE_WARNINGS` | `ON` | 是否为 CAORS 自有目标开启编译警告 |
| `CAORS_ENABLE_PEDANTIC_WARNINGS` | `OFF` | 是否启用严格标准扩展警告，例如 `-Wpedantic` |
| `CAORS_WARNINGS_AS_ERRORS` | `OFF` | 是否将 CAORS 自有目标的编译警告视为错误 |
| `USE_DEBUG_OUTPUT` | Debug preset 为 `ON` | 是否启用调试输出宏 |
| `USE_LOG` | Debug preset 为 `ON` | 是否启用日志宏 |
| `USE_LOG_SEVERITY` | `0` | 日志最低级别宏 |

开启警告即错误：

```sh
cmake --preset windows-clang-debug -DCAORS_WARNINGS_AS_ERRORS=ON
```

开启严格标准扩展警告：

```sh
cmake --preset windows-clang-debug -DCAORS_ENABLE_PEDANTIC_WARNINGS=ON
```

构建 benchmark：

```sh
cmake --preset windows-clang-debug -DCAORS_BUILD_BENCHMARKS=ON
cmake --build --preset windows-clang-debug --target caors_benchmarks
```

## 示例

示例说明见 [examples/README.md](../examples/README.md)。

在本项目 CMake 内部链接库目标：

```cmake
target_link_libraries(your_target PRIVATE caors::caors)
```

## 安装

构建后可安装到指定目录：

```sh
cmake --install build --prefix <install-prefix>
```

当前安装内容包括：

- 静态库文件。
- `include/caors/` 公共头文件，其中 `version.h` 由 `version.h.in` 生成。
- `lib/cmake/caors/caorsConfig.cmake` 包配置文件。
- `lib/cmake/caors/caorsConfigVersion.cmake` 版本匹配文件。
- `lib/cmake/caors/caorsTargets.cmake` 导出目标文件。
- `share/doc/caors/LICENSE` 许可证文件。

安装后，外部项目可通过 Config 模式查找并链接：

```cmake
find_package(caors CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE caors::caors)
```
