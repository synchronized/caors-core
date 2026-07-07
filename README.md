<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# caors

CAORS 是一个 C++20 基础设施库，面向游戏引擎、实时应用和数据驱动型工具开发。项目当前处于早期建设阶段，目标能力包括 core 基础设施、自定义内存分配器、类型元数据、对象系统、运行时反射和跨平台序列化。

## 项目定位

CAORS 目前更偏底层库和实验性基础设施，而不是完整引擎或应用框架。当前设计重点是：

- 提供可组合的内存资源、分配器和调试跟踪能力。
- 建立共享的类型元数据底座，避免对象系统、反射系统和序列化系统重复维护类型索引。
- 保持偏 STL / RTTR 的库风格，公共 API 使用小写下划线命名。
- 通过 CMake preset、示例和测试维持跨平台构建入口。

当前项目仍处于早期设计和验证阶段，暂不承诺公共 API 稳定性。现阶段修改时优先保证模块边界、
命名一致性、生命周期语义和长期架构清晰；必要时可以接受破坏性 API 调整。进入稳定版本前会重新明确兼容策略。

## 当前状态

当前仓库已实现：

- `caors` 库目标，别名目标为 `caors::caors`。
- 基础配置 API：`caors/config.h`。
- 统一错误 API：`caors/error.h`，支持轻量错误状态和 `std::error_code` 适配。
- 导出宏 API：`caors/export.h`。
- 基础版本 API：`caors/version.h`，由 `include/caors/version.h.in` 在配置阶段生成。
- Core API：`caors/core/string_name.h`、`caors/core/guid.h`、`caors/core/type_guid.h`、
  `caors/core/containers.h` 和 `caors/core/locator.h`。
- 基础 IO API：`caors/io.h`，包含虚拟路径、同步字节流、文件系统抽象、本地文件系统、内存文件系统和挂载文件系统。
- 事件 API：`caors/events.h`，包含轻量 delegate、同步 signal 和连接句柄。
- 类型元数据 API：`caors/meta.h`，包含运行期 `type_id` 和稳定 `type_guid`。
- 基础内存资源 API：`caors/memory.h`。
- 基础对象系统 API：`caors/object.h`。
- 基础反射 API：`caors/reflection.h`。
- 资源 API：`caors/resources.h`，包含资源身份、引用、运行期 handle、缓存、清单、loader/saver、同步 reload
  通知和字节来源 provider。
- 运行期值 API：`caors/value.h`，包含 `any`、`any_ref`、运行期 view 和 `variant` 数据 DOM。
- 序列化 API：`caors/serialization.h`，提供基于反射字段的文本/JSON 读写、DOM、codec、资源引用和多态读写入口。
- `examples/` 可运行示例。
- Catch2 测试骨架，测试目录按叶子主题自动生成独立测试目标。
- CMake install/export 基础配置。

具体可用 API 以源码和专题文档为准，路线规划见 [docs/roadmap.md](docs/roadmap.md)。

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

更完整的构建、安装和 CMake 选项说明见 [docs/build.md](docs/build.md)。

## 文档入口

- [docs/README.md](docs/README.md)：文档索引。
- [docs/build.md](docs/build.md)：构建、测试、安装和 CMake 选项。
- [docs/config.md](docs/config.md)：编译期配置常量、异常检测和内部分配策略。
- [docs/development.md](docs/development.md)：开发规范、代码格式和静态检查。
- [docs/error.md](docs/error.md)：统一错误模型和标准库错误码适配。
- [docs/events.md](docs/events.md)：轻量 delegate、同步 signal 和连接生命周期。
- [docs/io.md](docs/io.md)：同步字节流、文件系统抽象和本地文件系统实现。
- [docs/roadmap.md](docs/roadmap.md)：项目路线图。
- [docs/registration.md](docs/registration.md)：类型、对象、反射和序列化的注册顺序与初始化策略。
- [docs/serialization.md](docs/serialization.md)：序列化元信息、文本/JSON 读写、codec、资源引用和多态读写边界。
- [docs/value.md](docs/value.md)：`any`、`any_ref`、运行期 view 和 `variant` 数据 DOM。
- [examples/README.md](examples/README.md)：示例构建、运行和新增规则。
- [tests/README.md](tests/README.md)：测试组织、运行和新增规则。

## 许可证

CAORS 使用 MIT License 发布。详细内容见 [LICENSE](LICENSE)。
