<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Reflection

本文档说明 CAORS 反射系统的当前设计边界。当前反射提供类型和字段元信息注册、查询，以及基础字段读写；暂不提供方法调用、属性系统或代码生成。

## 设计目标

反射第一阶段解决几件事：

- 复用 `meta_type_data`、`meta_type`、`type_id` 和 `type_guid` 表达类型身份。
- 基于 `meta_registry` 把 `reflection_type_data` 挂载为类型元数据组件。
- 通过 RTTR 风格 builder 或底层字段列表描述字段。
- 提供按类型 id、类型名和稳定 GUID 查询反射类型。
- 提供全局 append-only 注册表，以及稳定的类型视图和字段视图。
- 提供字段基础读写入口和运行期值拷贝。

当前不会包含：

- 方法调用。
- 属性元数据。
- 自动代码扫描或代码生成。
- 容器、枚举和复杂类型转换规则。

## 核心概念

### 底层 reflection_field_data

`reflection_field_data` 是字段的底层存储描述，主要用于注册表内部保存字段数据，以及 `make_reflection_field_data<Field>()`
这类底层注册入口。外部查询和字段访问更推荐使用 `reflection_field`，避免直接依赖底层存储结构。

`reflection_field_data` 描述一个字段：

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

字段描述可以通过 `make_reflection_field_data<Field>()` 创建：

```cpp
struct actor {
  int id = 0;
  float health = 100.0F;
};

caors::reflection_field_data field =
    caors::make_reflection_field_data<float>("health", "float", offsetof(actor, health));
```

第一版要求调用方显式传入 offset，通常配合 `offsetof()` 使用。后续如果引入代码生成工具，可以由工具生成字段描述。

更推荐使用 `reflection::class_<T>()` builder，它可以通过成员指针推导字段大小、对齐、偏移和字段类型：

```cpp
caors::reflection::class_<actor>("game.actor")
    .field("id", &actor::id)
    .field("health", &actor::health)
    .commit();
```

字段类型推导规则：

- 如果字段类型已经通过模板注册函数注册到全局反射注册表，优先复用该类型的 `meta_type`。
- 如果字段类型没有注册，但存在 `type_name<T>` 默认映射，则使用默认类型名注册为基础类型，并使用注册表分配的 `type_id`。
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

推荐通过 `reflection_field` 读写字段：

```cpp
actor instance{};

auto reflected = caors::find_reflection("game.actor");
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
int source = 42;
id->try_set_value(&instance, caors::any::from(source));
```

如果只需要访问字段本体，或者字段不可拷贝，可以使用 `get_ref()` / `get_mutable_ref()` 获取
运行期引用：

```cpp
caors::any_ref ref = id->get_mutable_ref(&instance);

if (int* current = ref.try_get_mutable<int>()) {
  *current = 7;
}
```

`any_ref` 不拥有对象，也不会延长字段所属对象生命周期。调用方必须保证原对象在引用使用期间仍然存活。
`get_ref()` 始终返回只读引用；`get_mutable_ref()` 只会在字段可写且传入对象非空时返回可写引用。

### any

`any` 是运行期 C++ 类型值拷贝，保存注册表分配的类型 id、类型名、C++ 类型 key、对象拷贝、大小和对齐信息。
它不引用原对象，因此可以安全地脱离字段所属对象生命周期使用。

`any` 当前提供：

- `valid()`：判断值是否有效。
- `type()` / `type_name()`：查询运行期类型身份。
- `matches_type<T>()`：判断是否匹配指定 C++ 类型。
- `try_get<T>()`：获取只读指针。
- `try_get_mutable<T>()`：获取可写指针。
- `try_set<T>()`：从同类型 C++ 值写入当前 `any`。
- `try_set(any)`：从同类型运行期值写入当前 `any`。

旧 `caors::value` 仍作为 `caors::any` 的兼容别名保留。第一版只支持可拷贝类型；不可拷贝字段调用 `get_value()`
会返回无效 `any`。

### any_ref

`any_ref` 是运行期 C++ 类型引用，保存类型信息、对象地址、大小、对齐和可写性。它不拥有对象、
不复制对象，也不析构对象，适合零拷贝访问字段和不可拷贝字段。

`any_ref` 当前提供：

- `valid()`：判断引用是否有效。
- `writable()`：判断是否允许通过该引用写入。
- `type()` / `type_name()`：查询运行期类型身份。
- `matches_type<T>()`：判断是否匹配指定 C++ 类型。
- `try_get<T>()`：获取只读指针。
- `try_get_mutable<T>()`：获取可写指针；只读引用会返回 `nullptr`。

### reflection_type_data

`reflection_type_data` 是底层类型反射数据，只保存反射系统自己的内容，例如字段列表。类型身份由
`meta_type_data` / `meta_type` 统一管理。

```cpp
caors::reflection_type_data data = caors::make_reflection_type_data(
    {
        caors::make_reflection_field_data<int>("id", "int", offsetof(actor, id)),
        caors::make_reflection_field_data<float>("health", "float", offsetof(actor, health)),
    });
```

可以通过 `find_field()` 按字段名查询字段：

```cpp
auto field = data.find_field("health");
```

### reflection_type 和 reflection_field

`reflection_type` 和 `reflection_field` 是外部查询反射信息的推荐入口：

```cpp
auto reflected = caors::find_reflection("game.actor");
auto field = reflected->find_field("health");

actor instance{};
field->try_set<float>(&instance, 75.0F);
```

`reflection_type` 内部持有 `meta_type` 和 `reflection_type_data` 的只读指针。`reflection_field`
内部持有 `reflection_field_data` 的只读指针。反射注册表是进程级 append-only 注册表，
类型和字段注册后不可撤销、不可替换，因此有效视图可以在进程生命周期内稳定使用。

当前字段数据以 `std::vector<reflection_field_data>` 保存在 `reflection_type_data` 内。字段视图的
稳定性依赖“注册后冻结”约束：注册完成后不会再向该类型追加、删除或替换字段。如果后续支持热重载、
字段增量注册或组件替换，需要先重新设计字段存储和视图失效规则。

`reflection_type` 当前提供：

- `valid()`：判断视图是否仍有效。
- `info()` / `id()` / `name()` / `guid()`：查询类型身份。
- `field_count()` / `field_at()`：按索引遍历字段。
- `find_field()`：按字段名查询字段视图。

`reflection_field` 当前提供：

- `valid()`：判断视图是否仍有效。
- `name()` / `type()` / `type_name()` / `value_type()`：查询字段类型信息。
- `kind()` / `wrapper_semantics()` / `pointer_depth()` / `nullable()`：查询字段形态语义。
- `offset()` / `size()` / `alignment()` / `writable()`：查询字段布局和可写性。
- `try_get<T>()` / `try_set<T>()`：按静态类型读写字段。
- `get_value()` / `try_set_value()`：按运行期 `any` 读写字段。
- `get_ref()` / `get_mutable_ref()`：按运行期 `any_ref` 引用字段。

### 全局注册表

反射系统使用进程级全局注册表。普通代码通过 `reflection::class_<T>()`、`register_reflection<T>()`
和 `find_reflection()` 访问它，不需要创建 registry 对象：

```cpp
caors::error result =
    caors::reflection::class_<actor>("game.actor")
        .field("id", &actor::id)
        .commit();

auto reflected = caors::find_reflection("game.actor");
```

底层 `reflection_registry` 仍作为实现类型存在，并复用 `meta_registry` 保存基础类型信息，再把
`reflection_type_data` 作为组件挂载到同一个 `type_id` 上。注册表构造由库内部管理；注册后不提供
清理和撤销入口。

注册失败会返回 `caors::error`，`domain()` 为 `error_domain::reflection`，`value()` 为明确的通用错误码：

- `errc::invalid_type`：类型描述无效。
- `errc::invalid_field`：字段描述无效。
- `errc::duplicate_id`：同一个运行期 id、C++ 类型或等价类型描述已注册。
- `errc::duplicate_name`：同一个名称已注册。
- `errc::duplicate_guid`：同一个非 nil 稳定 GUID 已注册。
- `errc::duplicate_field`：同一个类型内存在重复字段名。
- `errc::duplicate_reflection`：目标类型已经挂载反射组件。
- `errc::inconsistent_type`：目标类型已存在，但基础 `meta_type_data` 不一致。
- `errc::missing_parent`：父类型未注册。

重复注册同一个类型时，如果基础类型信息一致且已经挂载反射组件，会返回 `duplicate_reflection`；
如果同名、同 GUID 或同 C++ 类型对应的基础 `meta_type_data` 不一致，会返回 `inconsistent_type` 或
对应的重复错误。

## 当前已实现能力

- `any`：运行期 C++ 类型值拷贝。
- `any_ref`：运行期 C++ 类型非拥有引用。
- `value`：`any` 的兼容别名。
- `reflection_field_data`：字段名、字段类型名、字段形态、wrapper 语义、解包类型、offset、size、alignment、
  基础 get/set 和运行期值访问。
- `reflection_type_data`：字段列表等反射组件数据。
- `reflection_type` / `reflection_field`：全局反射数据的推荐查询入口。
- `reflection_registry`：底层全局注册表实现。
- `reflection::class_<T>()`：RTTR 风格链式注册入口。
- `register_reflection<T>()`：底层字段列表注册入口。
- `find_reflection()`：在全局反射注册表中查询类型视图。

## 相关入口

- 公共头文件按职责拆分在 `include/caors/reflection/`：
  - `value.h`：兼容入口，转发到 `include/caors/value/any.h`。
  - `wrapper.h`：wrapper traits 和 wrapper 语义。
  - `field.h`：字段元信息和字段访问。
  - `type.h`：类型反射描述。
  - `view.h`：类型和字段的外部查询视图。
  - `registry.h`：全局反射注册表和注册查询函数。
  - `builder.h`：RTTR 风格链式注册入口。
- `include/caors/reflection.h` 是 umbrella 头，继续提供完整反射 API。
- `include/caors/value.h` 是 value 模块 umbrella 头，提供 `any` 和 `variant`。
- 可运行示例位于 `examples/reflection/`，示例组织规则见 [examples/README.md](../examples/README.md)。
- 行为测试位于 `tests/reflection/`，测试组织规则见 [tests/README.md](../tests/README.md)。
