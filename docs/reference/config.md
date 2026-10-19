<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Config

本文档说明 `caors/config.h` 暴露的编译期配置入口。配置项用于把平台、编译器和项目宏折叠成稳定的
`caors::config` 常量，供库内部和用户代码查询。

## 当前范围

当前已实现：

- `caors::config::has_exceptions`：当前编译单元是否启用 C++ 异常。
- `caors::config::internal_allocations_use_default_resource`：库内部全局对象是否使用 CAORS 默认
  `memory_resource`。
- `caors::config::default_new_alignment`：当前标准库默认 `operator new` 对齐值。

## 异常检测

`has_exceptions` 根据编译器特性宏推导，也可以通过 `CAORS_FORCE_DISABLE_EXCEPTIONS` 强制关闭：

```cpp
if constexpr (caors::config::has_exceptions) {
  // 可以使用异常路径。
}
```

需要注意：

- 该值描述当前编译配置，不在运行期变化。
- CAORS 公共 API 仍优先返回 `caors::error` 或 `caors::result<T>` 表达失败。
- 禁用异常时，调用方不应依赖抛异常路径处理错误。

## 内部分配策略

`internal_allocations_use_default_resource` 来自 `CAORS_INTERNAL_ALLOCATIONS_USE_DEFAULT_RESOURCE`：

```cpp
static_assert(!caors::config::internal_allocations_use_default_resource);
```

默认值为 `false`。这意味着 `string_name` intern 表、全局注册表等内部长期存活对象默认使用内部系统资源，
不跟随运行期 `set_default_memory_resource()` 切换，避免静态初始化阶段或销毁阶段出现分配和释放资源不匹配。

如果项目明确希望内部全局对象也走 CAORS 默认资源，可以在所有 CAORS 目标编译前统一定义：

```cmake
target_compile_definitions(your_target PRIVATE CAORS_INTERNAL_ALLOCATIONS_USE_DEFAULT_RESOURCE=1)
```

需要注意：

- 该宏必须在编译 CAORS 及使用相关头文件的目标时保持一致。
- 如果运行期切换默认资源，应保证切换前后已有内部对象的释放路径仍然可用。
- 一般应用不建议开启，除非已经明确掌控静态初始化、关闭顺序和默认资源生命周期。

## 默认 new 对齐

`default_new_alignment` 优先读取 `__STDCPP_DEFAULT_NEW_ALIGNMENT__`，缺失时回退到
`alignof(std::max_align_t)`：

```cpp
static_assert(caors::config::default_new_alignment >= alignof(std::max_align_t));
```

该常量用于内存模块判断默认分配对齐边界，不表示所有自定义后端都只能支持这个对齐。需要更高对齐时，
仍应通过 `memory_resource::allocate(size, alignment)` 或 `caors::allocate(size, alignment)` 显式传入。

相关测试位于 `tests/config/`，测试组织规则见 [tests/README.md](../../tests/README.md)。
