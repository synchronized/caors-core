<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# caors

CAORS 是一个 C++20 基础设施库，面向游戏引擎、实时应用和数据驱动型工具开发。项目当前处于早期建设阶段，目标能力包括 core 基础设施、自定义内存分配器、类型元数据、对象系统、meta 属性/方法和跨平台序列化。

## 项目定位

CAORS 目前更偏底层库和实验性基础设施，而不是完整引擎或应用框架。当前设计重点是：

- 提供可组合的内存资源、分配器和调试跟踪能力。
- 建立共享的类型元数据底座，避免对象系统、meta 属性/方法和序列化系统重复维护类型索引。
- 保持偏 STL / RTTR 的库风格，公共 API 使用小写下划线命名。
- 通过 CMake preset、示例和测试维持跨平台构建入口。

项目仍处于早期阶段，稳定性与兼容政策见 [API 稳定性](docs/reference/api-stability.md)。

## 当前状态

当前已提供 core、memory、math、IO、events、tasks、meta、objects、resources/assets 和序列化模块，
并配有可运行示例、行为测试和 CMake install/export。
具体 API、限制和阅读入口见[文档中心](docs/README.md)，后续方向见[路线图](docs/roadmap.md)。

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

在普通 PowerShell 中使用 Ninja + MSVC 时，通过包装脚本自动加载 Visual Studio 开发环境：

```powershell
.\scripts\msvc.ps1 -Preset windows-msvc-debug
.\scripts\msvc.ps1 -Preset windows-msvc-static-debug
```

不要在未初始化的普通终端中直接调用 `windows-msvc-*` preset；如果希望直接使用 CMake 命令，
可改用能够自行定位工具链的 `windows-vs2022-*` preset。

更完整的构建、安装和 CMake 选项说明见 [docs/guides/build.md](docs/guides/build.md)。

## 文档入口

- [docs/README.md](docs/README.md)：文档索引。
- [docs/guides/build.md](docs/guides/build.md)：构建、测试、安装和 CMake 选项。
- [docs/guides/development.md](docs/guides/development.md)：开发规范、代码格式和静态检查。
- [examples/README.md](examples/README.md)：示例构建、运行和新增规则。

模块参考、架构约束、规划与验证资料统一从文档索引进入。

## 许可证

CAORS 使用 MIT License 发布。详细内容见 [LICENSE](LICENSE)。
