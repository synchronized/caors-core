<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Examples

本目录存放 CAORS 的可运行示例。示例用于展示公共 API 的基础用法，不承担完整设计文档职责；更完整的主题说明见 [docs/README.md](../docs/README.md)。

## 构建

CAORS 作为顶层项目配置时，示例默认开启。也可以显式控制：

```sh
cmake --preset windows-clang-debug -DCAORS_BUILD_EXAMPLES=ON
cmake --preset windows-clang-debug -DCAORS_BUILD_EXAMPLES=OFF
```

构建所有示例：

```sh
cmake --build --preset windows-clang-debug
```

只构建某个示例目标：

```sh
cmake --build --preset windows-clang-debug --target caors_example_version_basic
```

## 目录

示例按主题目录组织：

- `alpha/`：注册、资源配置、asset manager、资源加载、引用解析和保存的最小闭环。
- `beta/`：beta 前主路径示例，包括资源同步加载、异步加载、同步重载和 factory option schema。
- `version/`：版本信息 API。
- `memory/`：内存资源、分配器、策略资源和调试跟踪。
- `meta/`：类型级 capability facade、运行期值和 descriptor-backed 能力查询。
- `object/`：对象注册、对象创建、生命周期管理器和销毁。
- `serialization/`：meta 属性序列化、普通类多态 wrapper、factory 和内存资源上下文。

具体示例以各主题目录下的 `*.cpp` 文件为准。

## 新增示例

新增示例时遵循以下规则：

- 按主题放入对应子目录，例如 `examples/memory/`。
- 新主题需要新增子目录和该目录下的 `CMakeLists.txt`。
- 示例目标命名使用 `caors_example_<主题>_<名称>`。
- 示例代码应尽量短，只展示一个明确场景。
- 基础示例优先展示公开入口和绑定后的 view，例如 `meta::type`、`any_ref::get_capability<Capability>()`
  或模块级 `find()` / `class_()` 入口；只有 capability 扩展、registry 管理或生命周期管理示例才直接展示
  类型级 capability、registry 或其他高级入口。
- 复杂背景说明放到 `docs/` 对应主题文档中，不写进示例源码。
