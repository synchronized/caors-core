<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Traits

本文档说明 CAORS 的底层扩展点。`traits` 模块只定义协议和主模板；标准库、第三方库或
CAORS 自身类型的具体适配放在 `adapters` 或对应主题模块中。

## 头文件边界

- `caors/traits.h`：聚合 traits 主模板。
- `caors/traits/type_name.h`：C++ 类型到 CAORS 类型名的默认映射。
- `caors/traits/wrapper.h`：wrapper 类型语义和内部值访问扩展点。
- `caors/traits/container.h`：顺序容器、集合、关联容器、pair、tuple 和 variant 语义扩展点。
- `caors/traits/object_ptr.h`：兼容转发头，对象指针扩展点实际位于 `caors/object/ptr.h`。
- `caors/adapters/stl.h`：标准库类型 traits 和 wrapper 适配聚合入口。
- `caors/adapters/stl_traits.h`：标准库容器、pair、tuple 和 variant 的 traits 特化。
- `caors/adapters/stl_wrapper.h`：标准库智能指针、`std::optional` 和
  `std::reference_wrapper` 的 wrapper traits 特化。

新代码应直接包含 `caors/traits/*.h` 或 `caors/adapters/*.h`。旧的 `meta` / `adapters`
转发头已经移除，避免用户在错误命名空间里特化 traits。

## type_name<T>

`traits::type_name<T>` 是 C++ 类型到默认类型名的映射。用户类型可以特化它，也可以在注册时直接传入名称。

```cpp
namespace caors::traits {

template <>
struct type_name<game::actor> {
  inline static constexpr std::string_view value = "game.actor";
};

} // namespace caors::traits
```

没有默认类型名的类型仍然可以通过显式注册使用：

```cpp
caors::meta::type_<game::actor>("game.actor");
```

## wrapper_traits<T>

`traits::wrapper_traits<T>` 描述某个类型是否是 wrapper，以及它的序列化语义。

```cpp
namespace caors::traits {

template <typename T>
struct wrapper_traits<game::box<T>> {
  static constexpr bool is_wrapper = true;
  static constexpr bool nullable = false;
  static constexpr wrapper_semantics semantics = wrapper_semantics::inline_value;
  using wrapped_type = T;
};

} // namespace caors::traits
```

`wrapper_semantics` 当前含义：

- `none`：不是 wrapper，也不是需要特殊处理的指针。
- `inline_value`：序列化时按内部 `T` 本体处理。
- `unsupported`：能识别形态，但当前不支持展开或序列化。

## wrapper_access<T>

`traits::wrapper_access<T>` 是可选访问适配。只要系统需要读取、修改、重置或按需构造 wrapper
内部值，就应同时特化它。

```cpp
namespace caors::traits {

template <typename T>
struct wrapper_access<game::box<T>> {
  using wrapped_type = T;

  static const wrapped_type* value(const game::box<T>& source) noexcept;
  static wrapped_type* mutable_value(game::box<T>& source) noexcept;
  static bool has_value(const game::box<T>& source) noexcept;
  static bool reset(game::box<T>& source) noexcept;
  static bool ensure_value(game::box<T>& source);
  static bool ensure_value(game::box<T>& source, caors::memory_resource* resource);
};

} // namespace caors::traits
```

第二个 `ensure_value` 重载是可选的。反序列化上下文提供 `memory_resource` 时，序列化层会优先调用
这个重载；没有提供时或 wrapper 不支持资源接管时，可以只实现普通 `ensure_value(source)`。

标准 wrapper 的资源语义：

- 普通 `std::unique_ptr<T>` 的 deleter 类型固定，空值构造和非多态读回保持标准 `new` / `delete`
  语义，不接管 `memory_resource`。
- `caors::memory::unique_ptr<T>` 使用可擦除 deleter。反序列化上下文提供 `memory_resource` 时，可以保存匹配的销毁回调。
- `std::shared_ptr<T>` 在上下文提供 `memory_resource` 且需要构造空内部值时，使用
  `std::allocate_shared<T>(caors::allocator<T>{resource})`。用户已经创建好的 `shared_ptr` 保持其原始控制块和分配策略。
- `std::optional<T>` 在自身对象内部原地构造 `T`，不单独分配；反序列化上下文提供 `memory_resource`
  时，会优先通过标准 uses-allocator 规则构造 allocator-aware 的 `T`。

对象引用不再作为 `wrapper_semantics` 的一种。需要按对象 id 序列化的引用类型应适配
`objects::detail::ptr_traits<T>` 和 `objects::detail::ptr_access<T>`，然后通过
`objects::ptr_view_of(any_ref)` 查询对象指针视图。

## 容器 traits

容器 traits 用于让 meta、reflection、serialization 和 value 模块以统一方式识别容器形态。

- `traits::sequence_traits<T>`：顺序容器，例如 vector、array、span。
- `traits::set_traits<T>`：集合容器，例如 unordered_set、set。
- `traits::associative_traits<T>`：关联容器，例如 unordered_map、map。
- `traits::pair_traits<T>`：二元组，例如 pair。
- `traits::tuple_traits<T>`：tuple-like 类型。
- `traits::variant_traits<T>`：variant-like 类型。

主模板放在 `traits` 模块中，具体特化应写在 `caors::traits` 命名空间下。标准库特化由
`caors/adapters/stl_traits.h` 提供。

顺序容器如果支持读入时调整大小，应提供 `resize(values, size)`。如果新增元素需要接收
`serialization::context::memory_resource`，可以额外提供资源感知重载：

```cpp
static bool resize(my_sequence& values, std::size_t size);
static bool resize(my_sequence& values, std::size_t size, caors::memory_resource* resource);
```

第二个重载是可选的。反序列化层在上下文提供 `memory_resource` 时会优先调用它；标准
`std::vector`、`std::deque`、`std::list` 和 `std::forward_list` 适配会用该资源构造新增的
allocator-aware 元素。该资源不会替换已经存在的容器 allocator，也不会改变已有元素的分配来源。

## 模块依赖方向

推荐依赖方向：

```text
core
  ↑
traits
  ↑
adapters / meta / value / reflection / serialization / object
```

`traits` 不应依赖 `meta`、`reflection` 或 `serialization`。当扩展点需要共享基础枚举或值类型时，
优先放在 `traits` 或 `core`，再由上层模块引用。
