<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Reflection

本文档说明 CAORS 反射系统的当前设计边界。当前反射提供类型和字段元信息注册、查询，以及基础字段读写；暂不提供方法调用、属性系统或代码生成。

## 设计目标

反射第一阶段解决几件事：

- 复用 `type_info`、`type_id` 和 `type_guid` 表达类型身份。
- 基于 `meta_registry` 把 `reflection_type` 挂载为类型元数据组件。
- 通过 RTTR 风格 builder 或底层字段列表描述字段。
- 提供按类型 id、类型名和稳定 GUID 查询反射类型。
- 提供绑定注册表生命周期的类型视图和字段视图。
- 提供字段基础读写入口和运行期值拷贝。

当前不会包含：

- 方法调用。
- 属性元数据。
- 自动代码扫描或代码生成。
- 容器、枚举和复杂类型转换规则。

## 核心概念

### 底层 field_info

`field_info` 是字段的底层存储描述，主要用于注册表内部保存字段数据，以及 `make_field_info<Field>()`
这类底层注册入口。外部查询和字段访问更推荐使用 `field_view`，避免长期持有注册表内部数据。

`field_info` 描述一个字段：

- `name`：字段名。
- `type`：字段声明类型的运行期 `type_id`。
- `type_name`：字段声明类型名。
- `kind`：字段形态，例如值、枚举、裸指针或 wrapper。
- `wrapper_semantics`：wrapper 或裸指针的序列化语义，例如内联值、对象引用或不支持。
- `value_type`：去掉一层指针或 wrapper 后的值类型。
- `pointer_depth`：当前支持的指针层级，合法值为 0 或 1。
- `nullable`：字段值是否可能为空。
- `offset`：字段在类型中的偏移。
- `size`：字段大小。
- `alignment`：字段对齐。
- `writable`：字段是否允许写入。

字段描述可以通过 `make_field_info<Field>()` 创建：

```cpp
struct actor {
  int id = 0;
  float health = 100.0F;
};

caors::field_info field =
    caors::make_field_info<float>("health", "float", offsetof(actor, health));
```

第一版要求调用方显式传入 offset，通常配合 `offsetof()` 使用。后续如果引入代码生成工具，可以由工具生成字段描述。

更推荐使用 `reflection::class_<T>()` builder，它可以通过成员指针推导字段大小、对齐、偏移和字段类型：

```cpp
caors::reflection::class_<actor>(registry, "game.actor")
    .field("id", &actor::id)
    .field("health", &actor::health)
    .commit();
```

字段类型推导规则：

- 如果字段类型已经通过模板注册函数注册到同一个反射注册表，优先复用该类型的 `type_info`。
- 如果字段类型没有注册，但存在 `type_name<T>` 默认映射，则使用默认类型名生成 `type_id`，并自动注册为基础类型。
- 如果两者都没有，`.commit()` 返回 `invalid_field`。
- 字段类型语义最多支持一层裸指针或 wrapper；`T**`、`std::unique_ptr<T*>`、
  `std::optional<std::shared_ptr<T>>` 这类嵌套组合会触发静态断言。

字段形态规则：

- `T`：`field_kind::value`，`value_type` 为 `T`。
- `enum`：`field_kind::enumeration`，`value_type` 为枚举类型。
- `T*`：`field_kind::raw_pointer`，`value_type` 为 `T`，`nullable` 为 `true`，
  `wrapper_semantics` 为 `inline_value`。
- `std::unique_ptr<T>`：`field_kind::wrapper`，`value_type` 为 `T`，`nullable` 为 `true`，
  `wrapper_semantics` 为 `inline_value`。
- `std::shared_ptr<T>`：`field_kind::wrapper`，`value_type` 为 `T`，`nullable` 为 `true`，
  `wrapper_semantics` 为 `inline_value`。
- `std::weak_ptr<T>`：`field_kind::wrapper`，`value_type` 为 `T`，`nullable` 为 `true`，
  `wrapper_semantics` 为 `inline_value`。
- `std::optional<T>`：`field_kind::wrapper`，`value_type` 为 `T`，`nullable` 为 `true`，
  `wrapper_semantics` 为 `inline_value`。
- `std::reference_wrapper<T>`：`field_kind::wrapper`，`value_type` 为 `T`，不表示空值，
  `wrapper_semantics` 为 `inline_value`。

STL 智能指针和裸指针只作为外部字段类型被反射系统描述。CAORS 不根据这些字段接管对象生命周期；
用户如果直接使用这些类型，需要自行保证释放、引用有效性和线程安全。后续 CAORS 自身的对象句柄、
引用计数或分配器类型会作为独立 wrapper 接入反射规则。

wrapper 识别通过 `wrapper_traits<T>` 扩展，而不是固定写死所有 wrapper 类型：

```cpp
namespace caors {

template <typename T>
struct wrapper_traits<my_box<T>> {
  static constexpr bool is_wrapper = true;
  static constexpr bool nullable = false;
  static constexpr field_wrapper_semantics semantics = field_wrapper_semantics::inline_value;
  using wrapped_type = T;
};

}
```

`field_wrapper_semantics` 当前含义：

- `none`：不是 wrapper，也不是需要特殊处理的指针。
- `inline_value`：序列化时按包装的 `T` 本体处理。`T*`、标准智能指针和 `optional<T>`
  默认都属于该语义。
- `object_ref`：序列化时按对象引用 id 处理，预留给后续 `caors::ref<T>` /
  `object_ref<T>` 这类类型。
- `unsupported`：明确识别为 wrapper，但当前序列化层不支持。

推荐通过 `field_view` 读写字段：

```cpp
actor instance{};

auto reflected = registry.find_type("game.actor");
auto id = reflected->find_field("id");
id->try_set<int>(&instance, 42);

auto value = id->try_get<int>(&instance);
```

`try_get<T>()` 在对象为空或类型不匹配时返回 `std::nullopt`。`try_set<T>()`
在对象为空、类型不匹配或字段不可写时返回 `false`。

也可以通过 `get_value()` 获取运行期值拷贝：

```cpp
caors::value value = id->get_value(&instance);

if (const int* current = value.try_get<int>()) {
  // 使用 current 读取字段值。
}

value.try_set(7);
```

`get_value()` 返回的是字段值拷贝，修改该 `value` 不会自动修改原对象。`try_set_value()`
可以从同类型 `value` 显式写回字段：

```cpp
int source = 42;
id->try_set_value(&instance, caors::value::from(source));
```

### value

`value` 是运行期值拷贝，保存类型 id、类型名、C++ 类型 key、对象拷贝、大小和对齐信息。
它不引用原对象，因此可以安全地脱离字段所属对象生命周期使用。

`value` 当前提供：

- `valid()`：判断值是否有效。
- `type()` / `type_name()`：查询运行期类型身份。
- `matches_type<T>()`：判断是否匹配指定 C++ 类型。
- `try_get<T>()`：获取只读指针。
- `try_get_mutable<T>()`：获取可写指针。
- `try_set<T>()`：从同类型 C++ 值写入当前 `value`。
- `try_set(value)`：从同类型运行期值写入当前 `value`。

这是 CAORS value/variant 层的第一步。第一版只支持可拷贝类型；不可拷贝字段调用 `get_value()`
会返回无效 `value`。后续如果需要零拷贝访问，会单独增加语义更明确的 view 类型，并在文档中明确
生命周期限制。

### reflection_type

`reflection_type` 是底层类型反射描述，包含基础 `type_info` 和字段列表：

```cpp
caors::reflection_type type = caors::make_reflection_type<actor>(
    "game.actor",
    {
        caors::make_field_info<int>("id", "int", offsetof(actor, id)),
        caors::make_field_info<float>("health", "float", offsetof(actor, health)),
    });
```

可以通过 `find_field()` 按字段名查询字段：

```cpp
auto field = type.find_field("health");
```

### reflection_type_view 和 field_view

`reflection_type_view` 和 `field_view` 是外部查询反射信息的推荐入口：

```cpp
auto reflected = registry.find_type("game.actor");
auto field = reflected->find_field("health");

actor instance{};
field->try_set<float>(&instance, 75.0F);
```

view 持有类型描述快照，并绑定产生它的 `reflection_registry`。当注册表销毁或调用 `clear()` 后，
已有 view 会失效，`valid()` 返回 `false`，字段读写也会失败。

`reflection_type_view` 当前提供：

- `valid()`：判断视图是否仍有效。
- `info()` / `id()` / `name()` / `guid()`：查询类型身份。
- `field_count()` / `field_at()`：按索引遍历字段。
- `find_field()`：按字段名查询字段视图。

`field_view` 当前提供：

- `valid()`：判断视图是否仍有效。
- `name()` / `type()` / `type_name()` / `value_type()`：查询字段类型信息。
- `kind()` / `wrapper_semantics()` / `pointer_depth()` / `nullable()`：查询字段形态语义。
- `offset()` / `size()` / `alignment()` / `writable()`：查询字段布局和可写性。
- `try_get<T>()` / `try_set<T>()`：按静态类型读写字段。
- `get_value()` / `try_set_value()`：按运行期 `value` 读写字段。

### reflection_registry

`reflection_registry` 复用 `meta_registry` 保存基础类型信息，并把 `reflection_type` 作为组件挂载到同一个 `type_id` 上：

```cpp
caors::reflection_registry registry;

caors::reflection_register_result result =
    caors::reflection::class_<actor>(registry, "game.actor")
        .field("id", &actor::id)
        .commit();

auto reflected = registry.find_type("game.actor");
```

注册失败会返回明确的 `reflection_register_result`：

- `success`：注册成功。
- `invalid_type`：类型描述无效。
- `invalid_field`：字段描述无效。
- `duplicate_id`：同一个运行期 id、C++ 类型或等价类型描述已注册。
- `duplicate_name`：同一个名称已注册。
- `duplicate_guid`：同一个非 nil 稳定 GUID 已注册。
- `duplicate_field`：同一个类型内存在重复字段名。
- `duplicate_reflection`：目标类型已经挂载反射组件。
- `inconsistent_type`：目标类型已存在，但基础 `type_info` 不一致。
- `missing_parent`：父类型未注册。

## 当前已实现能力

- `value`：运行期值拷贝。
- `field_info`：字段名、字段类型名、字段形态、wrapper 语义、解包类型、offset、size、alignment、
  基础 get/set 和运行期值访问。
- `reflection_type`：基础类型描述和字段列表。
- `field_view` / `reflection_type_view`：绑定注册表生命周期的推荐查询入口。
- `reflection_registry`：反射类型注册、查询、清理和统计。
- `reflection::class_<T>()`：RTTR 风格链式注册入口。
- `register_reflection<T>()`：底层字段列表注册入口。
- `find_reflection()`：在默认反射注册表中查询类型视图。

## 相关入口

- 公共头文件按职责拆分在 `include/caors/reflection/`：
  - `value.h`：运行期值拷贝。
  - `wrapper.h`：wrapper traits 和 wrapper 语义。
  - `field.h`：字段元信息和字段访问。
  - `type.h`：类型反射描述。
  - `view.h`：类型和字段的外部查询视图。
  - `registry.h`：反射注册表和底层注册函数。
  - `builder.h`：RTTR 风格链式注册入口。
- `include/caors/reflection.h` 是 umbrella 头，继续提供完整反射 API。
- 可运行示例位于 `examples/reflection/`，示例组织规则见 [examples/README.md](../examples/README.md)。
- 行为测试位于 `tests/reflection/`，测试组织规则见 [tests/README.md](../tests/README.md)。
