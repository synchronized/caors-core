<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# caors

CAORS 是一个 C++20 基础设施库模板，面向游戏引擎、实时应用和数据驱动型工具开发。项目当前处于早期建设阶段，已具备基础 CMake 工程结构、库目标、版本模块、Catch2 测试和安装导出骨架。

## 当前状态

当前仓库已实现：

- `caors` 静态库目标，别名目标为 `caors::caors`。
- 公共头文件目录 `include/caors/`。
- 基础配置 API：`caors/config.h`，集中放置编译期探测结果、异常开关和默认值。
- 导出宏 API：`caors/export.h`，提供 `CAORS_API` / `CAORS_LOCAL`。
- 基础版本 API：`caors/version.h`，由 `include/caors/version.h.in` 在配置阶段生成。
- 基础内存资源 API：`caors/memory.h`，包含后端资源、策略资源、包装资源、`allocator<T>` 和对象构造入口。
- `examples/` 可运行示例。
- Catch2 测试骨架，见tests目录。
- 目标级编译警告工具函数：`target_compile_warnings()`。
- CMake install/export 基础配置。

内存系统的设计说明见 [docs/memory.md](docs/memory.md)。

规划方向包括自定义内存分配器、对象系统、运行时类型反射和跨平台序列化。具体可用 API 以源码为准，README 不声明尚未实现的接口。

## TODO / 路线图

CAORS 后续按四个核心方向推进。每个方向应先补最小可用 API、测试和文档，再逐步扩展性能优化与工具链集成。

### 1. 定制化分配器

- [x] 定义统一的内存资源接口，明确分配、释放、对齐和错误处理语义。
- [x] 实现基础后端封装：system 后端和 callback 后端。
- [x] 提供 `try_allocate()` 不抛异常入口，并保留 `allocate()` 包装语义。
- [x] 提供 `allocator<T>`、`make<T>()` 和 `destroy<T>()` 基础入口。
- [x] 实现 `tracking_resource`、`monotonic_resource` 和 `fixed_block_pool`。
- [ ] 扩展栈式分配器、分级 size class 池等上层策略。
- [ ] 补充调用点追踪、泄漏检测和调试标记能力。
- [ ] 评估与 `std::pmr` 的适配方式。
- [ ] 增加跨平台压力测试和边界用例测试。

### 2. 对象系统

- [ ] 定义基础对象类型、对象标识和生命周期模型。
- [ ] 设计对象创建、销毁、引用和所有权约束。
- [ ] 提供轻量级类型层级和运行时查询能力。
- [ ] 明确线程安全边界，避免隐式全局状态。
- [ ] 增加对象生命周期、继承关系和错误路径测试。

### 3. 反射

- [ ] 定义类型、字段、函数和属性的元信息模型。
- [ ] 设计注册 API，支持手写注册和后续代码生成扩展。
- [ ] 提供运行时类型查询、字段访问和函数调用入口。
- [ ] 支持基础类型、枚举、类类型和容器类型的反射描述。
- [ ] 增加反射注册、查询、访问控制和类型转换测试。

### 4. 序列化

- [ ] 定义统一的序列化接口和错误模型。
- [ ] 支持基础类型、字符串、数组、对象和嵌套结构。
- [ ] 设计文本格式与二进制格式的扩展点。
- [ ] 与反射系统打通，实现基于元信息的对象序列化。
- [ ] 增加版本兼容、缺失字段、未知字段和跨平台字节序测试。

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

测试由 CTest 管理，Debug preset 提供对应测试 preset：

```sh
ctest --preset windows-clang-debug
ctest --preset linux-gcc-debug
```

关闭测试目标：

```sh
cmake --preset windows-clang-debug -DCAORS_BUILD_TESTING=OFF
```

测试框架使用仓库内置的 Catch2，目前版本位于 `3rd/catch-v3.15.0/`。

## CMake 选项

| 选项 | 默认值 | 说明 |
| --- | --- | --- |
| `BUILD_SHARED_LIBS` | `OFF` | 是否将 `caors` 构建为共享库 |
| `CAORS_BUILD_TESTING` | `ON` | 是否构建测试目标 |
| `CAORS_BUILD_EXAMPLES` | 顶层项目为 `ON` | 是否构建示例目标 |
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

## 示例

可运行示例位于 `examples/`，按主题组织：

- `examples/version/version_basic.cpp`：读取版本信息。
- `examples/memory/system_backend.cpp`：使用 system 内存后端。
- `examples/memory/callback_backend.cpp`：接入 callback 后端。
- `examples/memory/resource_layers.cpp`：组合使用 tracking、pool、allocator 和 make/destroy。

示例目标默认在 CAORS 作为顶层项目时构建，也可以显式开关：

```sh
cmake --preset windows-clang-debug -DCAORS_BUILD_EXAMPLES=ON
cmake --preset windows-clang-debug -DCAORS_BUILD_EXAMPLES=OFF
```

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

## 代码格式与静态检查

项目提供以下配置文件：

- `.clang-format`：基于 LLVM 风格微调，整体偏 STL/RTTR 这类库代码排版。
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

## 许可证

CAORS 使用 MIT License 发布。详细内容见 [LICENSE](LICENSE)。

## 开发规范

- 使用 C++20，不引入低于项目标准的兼容写法。
- 采用偏 STL/RTTR 的库风格：公共 API 使用小写下划线命名，例如 `get_version()`。
- 仅在单个 `.cpp` 文件内部使用的自由函数、变量和辅助类型应放入匿名 namespace，避免暴露翻译单元外符号。
- CMake 使用 target-based 写法，优先使用 `target_sources`、`target_include_directories`、`target_link_libraries` 和 `target_compile_features`。
- 公共头文件模板放在 `include/caors/`，生成头文件放在构建目录下。
- 跨库边界的公共类和非内联函数使用 `CAORS_API` 标记。
- 库实现放在 `src/caors/`。
- 版本号以根 `project(... VERSION ...)` 为唯一来源，不手写重复版本常量。
- 新增模块时同步补充测试目录和 `add_subdirectory()`。
- 第三方源码不默认启用项目级 `-Werror`，避免外部警告影响本项目构建。
