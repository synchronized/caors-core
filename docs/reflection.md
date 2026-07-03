<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Reflection

本文档说明 CAORS 反射系统的当前设计边界。当前反射提供类型和字段元信息注册、查询，以及基础字段读写；暂不提供方法调用、属性系统或代码生成。

## 设计目标

反射第一阶段解决几件事：

- 复用 `meta::type_data`、`meta::type_view`、`type_id` 和 `type_guid` 表达类型身份。
- 基于 `meta::registry` 把 `reflection::type_data` 挂载为类型元数据组件。
- 通过 RTTR 风格 builder 或底层字段列表描述字段。
- 提供按类型 id、类型名和稳定 GUID 查询反射类型。
- 提供全局 append-only 注册表，以及稳定的类型视图和字段视图。
- 提供字段基础读写入口和运行期值拷贝。

当前不会包含：

- 方法调用。
- 属性元数据。
- 自动代码扫描或代码生成。
- 动态多态类型记录和复杂类型转换规则。

## 核心概念

### 命名空间

反射模块的公开类型位于 `caors::reflection` 命名空间，例如 `reflection::field_data`、
`reflection::type_data`、`reflection::field`、`reflection::type` 和 `reflection::registry`。新代码应直接使用
命名空间内的短名称；旧的根命名空间低频别名已经移除。

### 底层 reflection::field_data

`reflection::field_data` 是字段的底层存储描述，主要用于注册表内部保存字段数据，以及 `reflection::make_field_data<Field>()`
这类底层注册入口。外部查询和字段访问更推荐使用 `reflection::field`，避免直接依赖底层存储结构。

`reflection::field_data` 描述一个字段：

- `name`：字段名。
- `type`：字段声明类型的运行期 `type_id`。
- `get_object_ref` / `set_object_ref`：对象引用 wrapper 的访问函数。
- `offset`：字段在类型中的偏移。
- `writable`：字段是否允许写入。

字段声明类型名、大小、对齐和 C++ 类型操作函数由 `type` 对应的 `meta::type_data` 查询。字段形态、
wrapper 语义、解包值类型、容器元素类型和 key/value 类型由同一 `type` 上挂载的
`meta::type_semantics_data` 查询，避免反射、序列化和 value 模块分别重复保存类型事实。

字段描述可以通过 `reflection::make_field_data<Field>()` 创建：

```cpp
struct actor {
  int id = 0;
  float health = 100.0F;
};

caors::reflection::field_data field =
    caors::reflection::make_field_data<float>("health", "float", offsetof(actor, health));
```

第一版要求调用方显式传入 offset，通常配合 `offsetof()` 使用。后续如果引入代码生成工具，可以由工具生成字段描述。

更推荐使用 `reflection::class_<T>()` builder，它可以通过成员指针推导偏移和字段类型：

```cpp
caors::reflection::class_<actor>("game.actor")
    .field("id", &actor::id)
    .field("health", &actor::health)
    .commit();
```

字段类型推导规则：

- 如果字段类型已经通过模板注册函数注册到全局反射注册表，优先复用该类型的 `meta::type_view`。
- 如果字段类型没有注册，但存在 `traits::type_name<T>` 默认映射，则使用默认类型名注册为基础类型，并使用注册表分配的 `type_id`。
- 如果字段类型满足 `traits::sequence_traits<T>::is_sequence`，反射 builder 会根据 traits 的类型名前缀和元素类型名生成
  容器类型名。字段类型注册到 meta 时会同步挂载元素类型语义。内置特化支持 `caors::vector<T>`、
  `std::vector<T, Allocator>`、`caors::list<T>`、`std::list<T, Allocator>`、`std::deque<T, Allocator>`、
  `std::array<T, N>` 和 `std::span<T, Extent>`。
- 如果字段类型满足 `traits::set_traits<T>::is_set`，反射 builder 会根据 traits 的类型名前缀和元素类型名生成
  容器类型名。字段类型注册到 meta 时会同步挂载元素类型语义。内置特化支持 `caors::hash_set<T>`、
  `std::unordered_set<T, ...>` 和 `std::set<T, ...>`。
- 如果字段类型满足 `traits::associative_traits<T>::is_associative`，反射 builder 会根据 traits 的类型名前缀、
  key 类型名和 value 类型名生成容器类型名。字段类型注册到 meta 时会同步挂载 key/value 类型语义。内置特化支持
  `caors::hash_map<K, V>`、`std::unordered_map<K, V, ...>` 和 `std::map<K, V, ...>`。
- 如果字段类型满足 `traits::pair_traits<T>::is_pair`，反射 builder 会根据 traits 的类型名前缀、first 类型名和
  second 类型名生成二元组类型名。内置特化支持 `std::pair<T, U>`。
- 如果字段类型满足 `traits::tuple_traits<T>::is_tuple`，反射 builder 会根据 traits 的类型名前缀和每个元素类型名
  生成 tuple-like 类型名。内置特化支持 `std::tuple<Ts...>`。
- 如果字段类型满足 `traits::variant_traits<T>::is_variant`，反射 builder 会根据 traits 的类型名前缀和每个候选类型名
  生成 variant-like 类型名。内置特化支持 `std::variant<Ts...>`。
- 如果以上规则都没有命中，`.commit()` 返回 `invalid_field`。
- 字段类型语义最多支持一层裸指针或 wrapper；`T**`、`std::unique_ptr<T*>`、
  `std::optional<std::shared_ptr<T>>` 这类嵌套组合会触发静态断言。

字段形态规则：

- `T`：`meta::type_kind::value`，`value_type` 为 `T`。
- `enum`：`meta::type_kind::enumeration`，`value_type` 为枚举类型。
- `T*`：`meta::type_kind::raw_pointer`，`value_type` 为 `T`，`nullable` 为 `true`，
  `traits::wrapper_semantics` 为 `inline_value`。
- `std::unique_ptr<T>`：`meta::type_kind::wrapper`，`value_type` 为 `T`，`nullable` 为 `true`，
  `traits::wrapper_semantics` 为 `inline_value`。
- `std::shared_ptr<T>`：`meta::type_kind::wrapper`，`value_type` 为 `T`，`nullable` 为 `true`，
  `traits::wrapper_semantics` 为 `inline_value`。
- `std::weak_ptr<T>`：`meta::type_kind::wrapper`，`value_type` 为 `T`，`nullable` 为 `true`，
  `traits::wrapper_semantics` 为 `unsupported`，不默认展开内部值。
- `std::optional<T>`：`meta::type_kind::wrapper`，`value_type` 为 `T`，`nullable` 为 `true`，
  `traits::wrapper_semantics` 为 `inline_value`。
- `std::reference_wrapper<T>`：`meta::type_kind::wrapper`，`value_type` 为 `T`，不表示空值，
  `traits::wrapper_semantics` 为 `inline_value`。
- `traits::sequence_traits<T>` 顺序容器：`meta::type_kind::sequence`，`value_type` 为元素类型；`element_type()`
  是对外语义别名。字段视图可以查询元素数量、访问元素引用，并在 traits 支持时执行清空、调整大小和追加元素。内置 `caors::vector<T>` 会生成
  `caors.vector<T>` 类型名。
- `traits::set_traits<T>` 集合容器：`meta::type_kind::set`，`value_type` 为元素类型；`element_type()` 是对外语义别名。内置
  `caors::hash_set<T>` 会生成 `caors.hash_set<T>` 类型名。
- `traits::associative_traits<T>` 关联容器：`meta::type_kind::associative`，`key_type` 为 key 类型，`value_type` 为
  mapped/value 类型；`mapped_type()` 是对外语义别名。内置 `caors::hash_map<K, V>` 会生成 `caors.hash_map<K,V>` 类型名。
- `traits::pair_traits<T>` 二元组：`meta::type_kind::pair`，`key_type` 为 first 类型，`value_type` 为 second 类型；
  `first_type()` / `second_type()` 是对外语义别名。内置 `std::pair<T, U>` 会生成 `std.pair<T,U>` 类型名。
- `traits::tuple_traits<T>` tuple-like 类型：`meta::type_kind::tuple`，元素类型由 `tuple_view::element_type(index)` 查询。
  内置 `std::tuple<Ts...>` 会生成 `std.tuple<Ts...>` 类型名。
- `traits::variant_traits<T>` variant-like 类型：`meta::type_kind::variant`，候选类型由
  `variant_view::alternative_type(index)` 查询，当前值类型由 `variant_view::value_type()` 查询。
  内置 `std::variant<Ts...>` 会生成 `std.variant<Ts...>` 类型名。

STL 智能指针和裸指针只作为外部字段类型被反射系统描述。CAORS 不根据这些字段接管对象生命周期；
用户如果直接使用这些类型，需要自行保证释放、引用有效性和线程安全。后续 CAORS 自身的对象句柄、
引用计数或分配器类型会作为独立 wrapper 接入反射规则。

wrapper 识别通过 `traits::wrapper_traits<T>` 扩展，而不是固定写死所有 wrapper 类型：

```cpp
namespace caors::traits {

template <typename T>
struct wrapper_traits<my_box<T>> {
  static constexpr bool is_wrapper = true;
  static constexpr bool nullable = false;
  static constexpr wrapper_semantics semantics = wrapper_semantics::inline_value;
  using wrapped_type = T;
};

template <typename T>
struct wrapper_access<my_box<T>> {
  using wrapped_type = T;

  static const wrapped_type* value(const my_box<T>& source) noexcept;
  static wrapped_type* mutable_value(my_box<T>& source) noexcept;
  static bool has_value(const my_box<T>& source) noexcept;
  static bool reset(my_box<T>& source) noexcept;
};

} // namespace caors::traits
```

`traits::wrapper_access<T>` 是可选扩展点。只需要识别 wrapper 形态时可以只特化 `traits::wrapper_traits<T>`；如果希望
`any_ref::create_indirect_view()` 或 `reflection::field::create_indirect_view()` 能访问内部值，则需要同时特化
`traits::wrapper_access<T>`。内置适配支持裸指针、`std::unique_ptr<T>`、`std::shared_ptr<T>`、
`std::optional<T>`、`std::reference_wrapper<T>` 和 `caors::objects::ref<T>` 的内部值访问；`std::weak_ptr<T>`
只提供 unsupported wrapper 语义，当前没有默认内部值访问，避免返回可能悬空的引用。

顺序容器识别通过 `traits::sequence_traits<T>` 扩展。traits 需要提供元素类型、类型名前缀和基础元素访问函数：

```cpp
namespace caors::traits {

template <typename T>
struct sequence_traits<my_sequence<T>> {
  static constexpr bool is_sequence = true;
  using element_type = T;

  static constexpr std::string_view type_name_prefix() noexcept { return "my.sequence"; }
  static std::size_t size(const my_sequence<T>& value) noexcept;
  static const element_type* element(const my_sequence<T>& value, std::size_t index) noexcept;
  static element_type* mutable_element(my_sequence<T>& value, std::size_t index) noexcept;
  static void clear(my_sequence<T>& value);
  static bool resize(my_sequence<T>& value, std::size_t size);
  static bool append(my_sequence<T>& value, const element_type& element);
};

}
```

`resize()` 适合只在元素可默认构造时返回成功，`append()` 适合只在元素可拷贝时返回成功。借用视图类型应额外提供
`static constexpr bool can_read = false`，避免序列化层默认读回到不受它管理的 backing storage；如果后续有明确字段策略，
可再通过显式 opt-in 配合 `read_in_place` 做原地读入。
调用方通过 `reflection::field` 操作 sequence 字段时，类型不匹配、对象为空或 traits 不支持对应操作都会返回失败。
内置适配已覆盖 `std::vector<T>`、`caors::vector<T>`、`std::list<T>`、`caors::list<T>`、
`std::forward_list<T>`、`std::deque<T>` 和 `std::array<T, N>`。`std::array<T, N>` 的类型名包含固定长度，例如
`std.array<int,3>`；读入或调整长度时只有 `size == N` 才会成功。`std::span<T>` 是借用视图，
类型名为 `std.span<T>`；固定长度 `std::span<T, N>` 的类型名为 `std.span<T,N>`，默认只用于反射访问和序列化写出。

集合容器识别通过 `traits::set_traits<T>` 扩展。集合只有元素，没有 mapped value；序列化层会按数组读写，
反序列化时遇到重复元素会返回失败：

```cpp
namespace caors::traits {

template <typename T>
struct set_traits<my_set<T>> {
  static constexpr bool is_set = true;
  using element_type = T;

  static constexpr std::string_view type_name_prefix() noexcept { return "my.set"; }
  static std::size_t size(const my_set<T>& value) noexcept;

  template <typename Visitor>
  static void for_each(const my_set<T>& value, Visitor&& visitor);

  static void clear(my_set<T>& value);
  static bool insert(my_set<T>& value, element_type element);
};

}
```

关联容器识别通过 `traits::associative_traits<T>` 扩展。traits 用于描述 key/value 类型、生成容器类型名，
并为通用 serialization codec 提供遍历和插入入口：

```cpp
namespace caors::traits {

template <typename K, typename V>
struct associative_traits<my_map<K, V>> {
  static constexpr bool is_associative = true;
  using key_type = K;
  using mapped_type = V;

  static constexpr std::string_view type_name_prefix() noexcept { return "my.map"; }
  static std::size_t size(const my_map<K, V>& value) noexcept;

  template <typename Visitor>
  static void for_each(const my_map<K, V>& value, Visitor&& visitor);

  static void clear(my_map<K, V>& value);
  static bool insert(my_map<K, V>& value, key_type key, mapped_type mapped);
};

}
```

二元组识别通过 `traits::pair_traits<T>` 扩展。traits 用于描述 first/second 类型、生成类型名，并给
`pair_view` 和序列化层提供访问入口：

```cpp
namespace caors::traits {

template <typename A, typename B>
struct pair_traits<my_pair<A, B>> {
  static constexpr bool is_pair = true;
  using first_type = A;
  using second_type = B;

  static constexpr std::string_view type_name_prefix() noexcept { return "my.pair"; }
  static const first_type* first(const my_pair<A, B>& value) noexcept;
  static first_type* mutable_first(my_pair<A, B>& value) noexcept;
  static const second_type* second(const my_pair<A, B>& value) noexcept;
  static second_type* mutable_second(my_pair<A, B>& value) noexcept;
};

}
```

内置适配已覆盖 `std::pair<T, U>`。pair 默认按二元素数组 `[first, second]` 序列化；反序列化时数组长度必须为 2。

tuple-like 类型识别通过 `traits::tuple_traits<T>` 扩展。traits 用于描述元素数量、元素类型、生成类型名，并给
`tuple_view` 和序列化层提供按下标访问入口：

```cpp
namespace caors::traits {

template <typename... Ts>
struct tuple_traits<my_tuple<Ts...>> {
  static constexpr bool is_tuple = true;
  static constexpr std::size_t size = sizeof...(Ts);

  template <std::size_t Index>
  using element_type = /* 第 Index 个元素类型 */;

  static constexpr std::string_view type_name_prefix() noexcept { return "my.tuple"; }

  template <std::size_t Index>
  static const element_type<Index>* element(const my_tuple<Ts...>& value) noexcept;

  template <std::size_t Index>
  static element_type<Index>* mutable_element(my_tuple<Ts...>& value) noexcept;
};

}
```

内置适配已覆盖 `std::tuple<Ts...>`。tuple 默认按数组 `[v0, v1, ...]` 序列化；反序列化时数组长度必须等于 tuple 元素数量。

variant-like 类型识别通过 `traits::variant_traits<T>` 扩展。traits 用于描述候选类型、生成类型名，并给
`variant_view` 和序列化层提供当前候选访问与切换入口：

```cpp
namespace caors::traits {

template <typename... Ts>
struct variant_traits<my_variant<Ts...>> {
  static constexpr bool is_variant = true;
  static constexpr std::size_t size = sizeof...(Ts);

  template <std::size_t Index>
  using alternative_type = /* 第 Index 个候选类型 */;

  static constexpr std::string_view type_name_prefix() noexcept { return "my.variant"; }
  static std::size_t index(const my_variant<Ts...>& value) noexcept;

  template <std::size_t Index>
  static const alternative_type<Index>* alternative(const my_variant<Ts...>& value) noexcept;

  template <std::size_t Index>
  static alternative_type<Index>* mutable_alternative(my_variant<Ts...>& value) noexcept;

  template <std::size_t Index>
  static bool emplace(my_variant<Ts...>& value);
};

}
```

内置适配已覆盖 `std::variant<Ts...>`。variant 默认按 tagged union 数组 `[index, value]` 序列化；
反序列化时数组长度必须等于 2，`index` 必须落在候选类型范围内。切换到某个候选类型时，候选类型需要支持默认构造。

`traits::wrapper_semantics` 当前含义：

- `none`：不是 wrapper，也不是需要特殊处理的指针。
- `inline_value`：序列化时按包装的 `T` 本体处理。`T*`、标准智能指针和 `optional<T>`
  默认都属于该语义。
- `object_ref`：序列化时按对象引用 id 处理。内置 `caors::objects::ref<T>` 属于该语义。
- `unsupported`：明确识别为 wrapper，但当前序列化层不支持。

推荐通过 `reflection::field` 读写字段：

```cpp
actor instance{};

auto reflected = caors::reflection::find("game.actor");
auto id = reflected->find_field("id");
id->try_set<int>(&instance, 42);

auto value = id->try_get<int>(&instance);
```

`try_get<T>()` 在对象为空或类型不匹配时返回 `std::nullopt`。`try_set<T>()`
在对象为空、类型不匹配或字段不可写时返回 `false`。

也可以通过 `get_value()` 获取运行期值拷贝：

```cpp
caors::any value = id->get_value(&instance);

if (const int* current = value.try_get<int>()) {
  // 使用 current 读取字段值。
}

value.try_set(7);
```

`get_value()` 返回的是字段值拷贝，修改该 `any` 不会自动修改原对象。`try_set_value()`
可以从同类型 `any` 显式写回字段：

```cpp
caors::meta::type<int>("int");

int source = 42;
id->try_set_value(&instance, caors::any::from(source));
```

如果只需要访问字段本体，或者字段不可拷贝，可以使用 `get_ref()` 获取运行期引用。传入
`const void*` 时返回只读引用，传入 `void*` 时返回可写引用：

```cpp
caors::any_ref ref = id->get_ref(&instance);

if (int* current = ref.try_get_mutable<int>()) {
  *current = 7;
}

ref.try_set(9);
```

`any_ref` 不拥有对象，也不会延长字段所属对象生命周期。调用方必须保证原对象在引用使用期间仍然存活。
`get_ref(const void*)` 始终返回只读引用；`get_ref(void*)` 只会在字段可写且传入对象非空时返回可写引用。

### any

`any` 是运行期 C++ 类型值拷贝，只保存注册表分配的类型 id 和对象拷贝。类型名、C++ 类型 key、大小、
对齐、复制、析构和赋值函数通过同一 `type_id` 的 `meta::type_data` 查询。
它不引用原对象，因此可以安全地脱离字段所属对象生命周期使用。

`any` 当前提供：

- `valid()`：判断值是否有效。
- `type()` / `type_name()`：查询运行期类型身份。
- `matches_type<T>()`：判断是否匹配指定 C++ 类型。
- `try_get<T>()`：获取只读指针。
- `try_get_mutable<T>()`：获取可写指针。
- `try_set<T>()`：从同类型 C++ 值写入当前 `any`。
- `try_set(any)`：从同类型运行期值写入当前 `any`。
- `create_sequence_view()`：如果当前值的类型挂载了顺序容器 metadata，则创建顺序容器视图。
- `create_set_view()`：如果当前值的类型挂载了集合容器 metadata，则创建集合容器视图。
- `create_associative_view()`：如果当前值的类型挂载了关联容器 metadata，则创建可写或只读关联容器视图。
- `create_indirect_view()`：如果当前值的类型挂载了指针或 wrapper 语义 metadata，则创建内部值视图。

第一版 `any` 只支持可拷贝类型；不可拷贝字段调用 `get_value()` 会返回无效 `any`。

### any_ref

`any_ref` 是运行期 C++ 类型引用，只保存类型 id、对象地址和可写性。类型细节同样通过 meta 查询。
它不拥有对象，不复制对象，也不析构对象，适合零拷贝访问字段和不可拷贝字段。

`any_ref` 当前提供：

- `valid()`：判断引用是否有效。
- `writable()`：判断是否允许通过该引用写入。
- `type()` / `type_name()`：查询运行期类型身份。
- `matches_type<T>()`：判断是否匹配指定 C++ 类型。
- `try_get<T>()`：获取只读指针。
- `try_get_mutable<T>()`：获取可写指针；只读引用会返回 `nullptr`。
- `try_set<T>()` / `try_set(any)` / `try_set(any_ref)`：从同类型值或引用写入当前引用指向的对象。
- `create_sequence_view()`：如果当前引用的类型挂载了顺序容器 metadata，则创建顺序容器视图。
- `create_set_view()`：如果当前引用的类型挂载了集合容器 metadata，则创建集合容器视图。
- `create_associative_view()`：如果当前引用的类型挂载了关联容器 metadata，则创建关联容器视图。
- `create_indirect_view()`：如果当前引用的类型挂载了指针或 wrapper 语义 metadata，则创建内部值视图。

### reflection::type_data

`reflection::type_data` 是底层类型反射数据，只保存反射系统自己的内容，例如字段列表。类型身份由
`meta::type_data` / `meta::type_view` 统一管理。

```cpp
caors::reflection::type_data data = caors::reflection::make_type_data(
    {
        caors::reflection::make_field_data<int>("id", "int", offsetof(actor, id)),
        caors::reflection::make_field_data<float>("health", "float", offsetof(actor, health)),
    });
```

可以通过 `find_field()` 按字段名查询字段：

```cpp
auto field = data.find_field("health");
```

### reflection::type 和 reflection::field

`reflection::type` 和 `reflection::field` 是外部查询反射信息的推荐入口：

```cpp
auto reflected = caors::reflection::find("game.actor");
auto field = reflected->find_field("health");

actor instance{};
field->try_set<float>(&instance, 75.0F);
```

`reflection::type` 内部持有 `meta::type_view` 和 `reflection::type_data` 的只读指针。`reflection::field`
内部持有 `reflection::field_data` 的只读指针。反射注册表是进程级 append-only 注册表，
类型和字段注册后不可撤销、不可替换，因此有效视图可以在进程生命周期内稳定使用。

当前字段数据以 `caors::vector<reflection::field_data>` 保存在 `reflection::type_data` 内。字段视图的
稳定性依赖“注册后冻结”约束：注册完成后不会再向该类型追加、删除或替换字段。如果后续支持热重载、
字段增量注册或组件替换，需要先重新设计字段存储和视图失效规则。

`reflection::type` 当前提供：

- `valid()`：判断视图是否仍有效。
- `info()` / `id()` / `name()` / `guid()`：查询类型身份。
- `field_count()` / `field_at()`：按索引遍历字段。
- `find_field()`：按字段名查询字段视图。

`reflection::field` 当前提供：

- `valid()`：判断视图是否仍有效。
- `name()` / `type()` / `type_name()`：查询字段声明类型信息。
- `value_type()`：查询统一 value 类型。对普通值是字段自身类型；对指针和 wrapper 是内部值类型；对 sequence/set 是元素类型；对 associative 是 mapped/value 类型；对 pair 是 second 类型。
- `element_type()` / `element_type_name()`：查询 sequence/set 字段的元素类型信息，是 `value_type()` 的语义别名。
- `key_type()` / `key_type_name()`：查询 associative 字段的 key 类型信息；pair 字段内部也复用它保存 first 类型。
- `first_type()` / `first_type_name()`：查询 pair 字段的 first 类型信息。
- `mapped_type()` / `mapped_type_name()`：查询 associative 字段的 mapped/value 类型信息，是 `value_type()` 的语义别名。
- `second_type()` / `second_type_name()`：查询 pair 字段的 second 类型信息，是 `value_type()` 的语义别名。
- `kind()` / `wrapper_semantics()` / `pointer_depth()` / `nullable()`：查询字段形态语义。
- `offset()` / `size()` / `alignment()` / `writable()`：查询字段布局和可写性。
- `try_get<T>()` / `try_set<T>()`：按静态类型读写字段。
- `get_value()` / `try_set_value()`：按运行期 `any` 读写字段。
- `get_ref(const void*)` / `get_ref(void*)`：按运行期 `any_ref` 引用字段，权限由对象指针的 constness 决定。
- `create_indirect_view(const void*)` / `create_indirect_view(void*)`：创建指针或 wrapper 字段的内部值视图。
- `is_sequence()`：判断字段是否是顺序容器字段。
- `is_set()`：判断字段是否是集合容器字段。
- `is_associative()`：判断字段是否是关联容器字段。
- `is_pair()`：判断字段是否是二元组字段。
- `is_tuple()`：判断字段是否是 tuple-like 字段。
- `is_variant()`：判断字段是否是 variant-like 字段。

`sequence_view` 当前提供：

- `valid()` / `writable()`：判断视图有效性和是否允许修改。
- `value_type()` / `value_type_name()`：查询元素类型信息。
- `element_type()` / `element_type_name()`：查询元素类型信息，是 `value_type()` 的语义别名。
- `size()` / `element_ref()` / `begin()` / `end()`：查询和访问顺序容器元素。可写视图返回的元素引用也可写。
- `clear()` / `resize()` / `append_value()`：通过 `traits::sequence_traits` 修改顺序容器。只有可写视图允许修改。

`set_view` 当前提供：

- `valid()` / `writable()`：判断视图有效性和是否允许修改。
- `value_type()` / `value_type_name()`：查询元素类型信息。
- `element_type()` / `element_type_name()`：查询元素类型信息，是 `value_type()` 的语义别名。
- `size()` / `begin()` / `end()` / `for_each_ref()`：查询和遍历集合。集合元素按 key 语义暴露为只读 `any_ref`。
- `clear()` / `insert_value()`：通过 `traits::set_traits` 修改集合。只有可写视图允许修改。

`associative_view` 当前提供：

- `valid()` / `writable()`：判断视图有效性和是否允许修改。
- `key_type()` / `key_type_name()` / `value_type()` / `value_type_name()`：查询 key/value 类型信息。
- `mapped_type()` / `mapped_type_name()`：查询 mapped/value 类型信息，是 `value_type()` 的语义别名。
- `size()` / `begin()` / `end()` / `for_each_ref()`：查询和遍历关联容器。iterator 解引用返回 `associative_entry_ref`，其中 key/value 都是只读 `any_ref`。
- `clear()` / `insert_value()`：通过 `traits::associative_traits` 修改关联容器。只有可写视图允许修改；插入时
  key 和 value 必须与类型级关联容器 metadata 记录的 `type_id` 一致。

`set_view` 和 `associative_view` 的 iterator 不拥有底层容器，也不延长 view 或容器对象生命周期。
保存 iterator 时，调用方需要保证对应 view 和容器仍然有效；如果类型擦除 iterator 创建失败，遍历会安全退化为空 range。

`pair_view` 当前提供：

- `valid()` / `writable()`：判断视图有效性和是否允许修改。
- `first_type()` / `first_type_name()`：查询 first 类型信息。
- `value_type()` / `value_type_name()`：查询 second 的统一 value 类型信息。
- `second_type()` / `second_type_name()`：查询 second 类型信息，是 `value_type()` 的语义别名。
- `first_ref()` / `second_ref()` / `value_ref()`：获取 first 或 second 的 `any_ref`。只读视图返回只读引用；可写视图在 traits 支持时返回可写引用。
- `set_first()` / `set_second()`：用同类型 `any` 写入 first 或 second。只有可写视图允许修改。

`tuple_view` 当前提供：

- `valid()` / `writable()`：判断视图有效性和是否允许修改。
- `size()`：查询 tuple 元素数量。
- `element_type(index)` / `element_type_name(index)`：查询指定下标的元素类型信息。
- `element_ref(index)`：获取指定元素的 `any_ref`。只读视图返回只读引用；可写视图在 traits 支持时返回可写引用。
- `set_element(index, value)`：用同类型 `any` 写入指定元素。只有可写视图允许修改。

`variant_view` 当前提供：

- `valid()` / `writable()`：判断视图有效性和是否允许修改。
- `size()`：查询候选类型数量。
- `index()`：查询当前候选下标；如果底层 variant 处于无值状态则返回空。
- `alternative_type(index)` / `alternative_type_name(index)`：查询指定候选类型信息。
- `value_type()` / `value_type_name()`：查询当前候选值类型信息。
- `value_ref()`：获取当前候选值的 `any_ref`。只读视图返回只读引用；可写视图在 traits 支持时返回可写引用。
- `emplace(index)`：切换当前候选。只有可写视图允许修改，目标候选需要支持默认构造。
- `set_value(value)`：用同类型 `any` 写入当前候选值。只有可写视图允许修改。

`indirect_view` 当前提供：

- `valid()` / `writable()`：判断视图有效性和是否允许修改。
- `value_type()` / `value_type_name()`：查询内部值类型。
- `has_value()`：判断指针或 wrapper 当前是否持有内部值。
- `value_ref()`：获取内部值 `any_ref`。只读视图返回只读引用；可写视图在 wrapper 支持时返回可写引用。
- `reset()`：把裸指针或 nullable wrapper 置空。只有可写视图允许重置。

`indirect_view` 不拥有 wrapper 或内部值，也不延长内部对象生命周期。裸指针和智能指针解包后返回的是当前
指向对象的引用；调用方需要保证引用使用期间对象仍然有效。

`meta::sequence_data`、`meta::set_data`、`meta::associative_data`、`meta::pair_data`、`meta::tuple_data` 和
`meta::variant_data` 只保存 value/key、元素或候选类型的 `type_id`
以及容器操作函数。value/key 的名称、C++ 类型 key、大小、对齐和复制/赋值函数统一从
基础 `meta::type_data` 查询，避免容器 metadata 与基础类型 metadata 重复维护。

容器、pair、tuple 和 variant 视图不要求值一定来自反射字段。反射字段场景可以先通过 `get_ref()` 创建 `any_ref`，
再调用对应的 `create_*_view()`；普通 `any` / `any_ref` 也可以直接创建视图：

```cpp
caors::value::sequence_view field_sequence =
    field->get_ref(&instance).create_sequence_view();
caors::value::associative_view field_map =
    field->get_ref(&instance).create_associative_view();

caors::meta::type<caors::hash_map<int, int>>("caors.hash_map<int,int>");

caors::hash_map<int, int> values;
caors::any_ref ref = caors::any_ref::from(values, "caors.hash_map<int,int>");
caors::value::associative_view map = ref.create_associative_view();
map.insert_value(caors::any::from(1), caors::any::from(2));

for (caors::value::associative_entry_ref entry : map) {
  caors::any_ref key = entry.key();
  caors::any_ref value = entry.value();
}
```

### 全局注册表

反射系统使用进程级全局注册表。普通代码通过 `reflection::class_<T>()` 和 `reflection::find()`
访问它，不需要创建 registry 对象：

```cpp
caors::meta::type_view type = caors::meta::type<actor>("game.actor").get();

caors::error result =
    caors::reflection::class_<actor>(type)
        .field("id", &actor::id)
        .commit();

auto reflected = caors::reflection::find("game.actor");
```

推荐先通过 `meta::type<T>("name")` 注册基础类型，再把 `caors::meta::get<T>()` 或保存下来的 `meta::type_view` 传给
`reflection::class_<T>(meta::type_view)`。这条路径只向已注册类型挂载 `reflection::type_data`，不会创建或修改
基础类型记录；传入无效 `meta::type_view` 会返回 `errc::invalid_type`，传入不存在的 `type_id` 会返回
`errc::missing_type`。低层反射组件注册函数属于内部实现入口，普通代码不直接调用。

反射注册应在 `meta::freeze_registry()` 前完成。全局 meta 注册表冻结后，继续注册反射组件会返回
`errc::frozen`。

底层 `reflection::registry` 仍作为实现类型存在，并复用 `meta::registry` 保存基础类型信息，再把
`reflection::type_data` 作为组件挂载到同一个 `type_id` 上。注册表构造由库内部管理；注册后不提供
清理和撤销入口。

注册失败会返回 `caors::error`，`domain()` 为 `error_domain::reflection`，`value()` 为明确的通用错误码：

- `errc::invalid_type`：类型描述无效。
- `errc::invalid_field`：字段描述无效。
- `errc::duplicate_id`：同一个运行期 id、C++ 类型或等价类型描述已注册。
- `errc::duplicate_name`：同一个名称已注册。
- `errc::duplicate_guid`：同一个非 nil 稳定 GUID 已注册。
- `errc::duplicate_field`：同一个类型内存在重复字段名。
- `errc::duplicate_reflection`：目标类型已经挂载反射组件。
- `errc::inconsistent_type`：目标类型已存在，但基础 `meta::type_data` 不一致。
- `errc::missing_parent`：父类型未注册。
- `errc::frozen`：全局 meta 注册表已冻结。

重复注册同一个类型时，如果基础类型信息一致且已经挂载反射组件，会返回 `duplicate_reflection`；
如果同名、同 GUID 或同 C++ 类型对应的基础 `meta::type_data` 不一致，会返回 `inconsistent_type` 或
对应的重复错误。

## 当前已实现能力

- `any`：运行期 C++ 类型值拷贝。
- `any_ref`：运行期 C++ 类型非拥有引用。
- `sequence_view` / `set_view` / `associative_view` / `pair_view` / `tuple_view` / `variant_view`：容器、二元组、tuple-like 和 variant-like 类型的运行期视图。
- `reflection::field_data`：字段名、字段声明类型 id、offset、可写性、对象引用访问函数和基础字段访问。
- `meta::type_semantics_data`：字段声明类型的形态、wrapper 语义、统一 value 类型和关联容器 key 类型。
- `reflection::type_data`：字段列表等反射组件数据。
- `reflection::type` / `reflection::field`：全局反射数据的推荐查询入口。
- `reflection::registry`：底层全局注册表实现。
- `reflection::class_<T>()`：RTTR 风格链式注册入口。
- `reflection::class_<T>(meta::type_view)`：推荐的显式 meta 注册入口，只挂载反射组件。
- `reflection::find()` / `reflection::contains()`：反射组件查询入口。

## 相关入口

- 公共头文件按职责拆分在 `include/caors/reflection/`：
  - `field.h`：字段元信息和字段访问。
  - `type.h`：类型反射描述。
  - `view.h`：类型和字段的外部查询视图。
  - `registry.h`：全局反射注册表和注册查询函数。
  - `builder.h`：RTTR 风格链式注册入口。
- `include/caors/reflection.h` 是 umbrella 头，继续提供完整反射 API。
- `include/caors/traits/*.h` 提供 `type_name`、wrapper、容器、pair、tuple 和 variant 类型语义扩展点主模板。
- `include/caors/traits/object_ref.h` 提供 `objects::ref<T>` 的 wrapper 语义适配。
- `include/caors/adapters/stl_traits.h` 和 `include/caors/adapters/stl_wrapper.h` 提供 `std::vector`、`std::deque`、`std::list`、`std::forward_list`、`std::array`、`std::span`、
  `std::pair`、`std::tuple`、`std::variant`、`std::set`、`std::unordered_set`、`std::map`、`std::unordered_map` 以及 CAORS 容器别名的默认 traits 特化。
- `include/caors/adapters/stl.h` 是 STL/CAORS 容器、wrapper 和反射 traits 适配的聚合入口，不包含序列化 codec。
- `include/caors/value.h` 是 value 模块 umbrella 头，提供 `any`、各类运行期 view 和
  `caors::variant` DOM 值；具体 view 头位于 `include/caors/value/`。
- 可运行示例位于 `examples/reflection/`，示例组织规则见 [examples/README.md](../examples/README.md)。
- 行为测试位于 `tests/reflection/`，测试组织规则见 [tests/README.md](../tests/README.md)。
