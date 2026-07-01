<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Serialization

本文档说明 CAORS 序列化层的当前 MVP。当前实现提供基于反射字段的简单文本序列化、DOM 中间对象和
基础 DOM 反序列化，用于验证 `meta_registry`、反射字段访问和统一错误模型之间的协作边界。

## 设计目标

第一阶段解决几件事：

- 用 `serialization_type_data` 表示类型的序列化元信息。
- 用 `serialization_field_data` 表示字段级序列化配置，例如字段别名、稳定字段 id、跳过和版本信息。
- 基于全局 `meta_registry` 挂载序列化组件，避免单独维护类型索引。
- 提供 `serialization_registry` 和 `register_serialization<T>()` 注册入口。
- 提供 `serialization_writer` 后端接口，让不同格式复用同一套字段遍历逻辑。
- 提供 `serialization_reader` 后端接口，让不同格式复用同一套字段读入逻辑。
- 提供 `variant` 中间对象树，方便工具、调试和多格式转换。
- 提供 `text_serialization_writer` 和 `serialize_text()`，通过反射字段读取对象并输出简单文本结构。
- 提供 `json_serialization_writer`、`serialize_json()`、`parse_json()` 和 `deserialize_json()`。
- 提供 `variant_serialization_reader` 和 `deserialize()`，通过 DOM 中间对象写回基础反射字段。
- 提供 `serialization_context`，支持按 schema version 过滤字段。
- 提供 `serialization_codec_registry`，允许把特定 C++ 类型作为原子序列化值读写。
- 支持已注册反射和序列化元信息的值类型结构体字段递归序列化和 DOM 反序列化。
- 提供基于 traits 的顺序容器、集合容器和关联容器 codec，支持标量、wrapper、对象引用和已注册结构体元素。
- 通过 `serialization_reference_resolver` 支持 `caors::object_ref<T>` 对象引用字段。
- 用 `caors::error` 和 `caors::result<T>` 表达失败状态和携带值的返回结果。

当前不会包含：

- 内置二进制格式。
- 版本迁移和默认值恢复。
- 完整 JSON 标准扩展，例如非 ASCII `\uXXXX` 解码。
- 多层指针、多层 wrapper、动态多态类型记录和对象图身份保持。
- `std::span`、`std::pair`、`std::tuple`、`std::variant` 等复合类型默认适配。

这些能力后续应在对象 handle、字段策略和格式扩展点明确后逐步补充。

## 核心概念

### 文件组织

序列化模块按职责拆分公共头文件：

- `caors/serialization/types.h`：格式枚举、后端能力、字段/类型配置、上下文和结果类型。
- `caors/serialization/registry.h`：序列化组件注册表和 `register_serialization()`。
- `caors/serialization/writer.h`：`serialization_writer`、`text_serialization_writer` 和
  `json_serialization_writer`。
- `caors/serialization/reader.h`：`serialization_reader`。
- `caors/serialization/codec.h`：类型级 codec 扩展点和 codec 注册表。
- `caors/serialization/dom_reader.h`：从 `variant` DOM 读入的 reader 后端。
- `caors/serialization/operations.h`：`serialize()`、`deserialize()`、`to_serialization_value()`、`serialize_text()` 等调度入口。
- `caors/serialization/vector_codec.h`：基于 `sequence_traits<T>` 的通用数组 codec，以及顺序容器默认适配。
- `caors/serialization/set_codec.h`：基于 `set_traits<T>` 的通用集合 codec，以及集合容器默认适配。
- `caors/serialization/map_codec.h`：基于 `associative_traits<T>` 的通用字典 codec，以及关联容器默认适配。
- `caors/serialization/stl_codecs.h`：STL/CAORS 容器 codec 聚合入口。
- `caors/serialization.h`：兼容用聚合头，包含以上所有入口。
- `caors/adapters/stl.h`：STL/CAORS 容器反射 traits 和序列化 codec 的一站式适配入口。

实现文件按 registry、codec、text writer、JSON writer/parser、DOM/value、writer 接口、DOM
写出、对象序列化、对象反序列化、中间值构造和文本便捷入口拆分在
`src/caors/serialization/`。共享的字段校验、schema version
过滤和标量读写 helper 保留在私有 `src/caors/serialization/serialization_internal.*`。

### serialization_type_data

`serialization_type_data` 是挂载在 `meta_registry` 上的序列化组件。它保存默认格式和字段级
序列化配置：

```cpp
caors::register_serialization<actor>({
    caors::make_serialization_field("health", "hp"),
    caors::make_serialization_field("debug_only", {}, true),
});
```

字段配置按字段名引用反射字段，但注册时不会要求反射字段已经存在。因此序列化注册可以先于
反射字段注册执行。真正序列化时才会查找反射字段；如果字段配置找不到对应反射字段，会返回
`errc::not_found`。

如果 `serialization_type_data::fields` 为空，`serialize_text()` 会回退到输出全部反射字段。

### serialization_field_data

`serialization_field_data` 描述单个字段的序列化规则：

- `field_name`：反射字段名。
- `serialized_name`：输出字段名；为空时使用 `field_name`。
- `field_id`：稳定字段编号，供紧凑二进制、数字键 msgpack 等不写字段名的格式使用。
- `version_added` / `version_removed`：配合 `serialization_context::schema_version` 做字段版本过滤。
- `required`：反序列化时缺失该字段会返回 `errc::not_found`。
- `skipped`：序列化和反序列化时跳过该字段。

`serialization_context::schema_version == 0` 表示不启用版本过滤，保持兼容默认行为。传入非 0 版本时：

- `version_added != 0 && schema_version < version_added` 的字段会被跳过。
- `version_removed != 0 && schema_version >= version_removed` 的字段会被跳过。

序列化字段配置属于 serialization 组件，不直接写入 `reflection_field_data`，也不持有
`reflection_field_data` 指针。反射负责描述字段是什么，序列化负责描述字段如何存储；真正序列化时
再按字段名解析反射字段，因此序列化注册和反射注册没有固定顺序要求。

### serialization_writer

`serialization_writer` 是格式后端接口。序列化调度层负责查找 `meta_registry`、匹配反射字段和读取
字段值；writer 只负责把对象、字段和值写成目标格式。

writer 通过 `serialization_format_features` 声明能力：

- `named_fields`：格式需要字段名，例如 text/json/msgpack map。
- `numeric_fields`：格式需要字段 id，例如紧凑二进制或数字键 map。
- `ordered_fields`：格式按字段顺序写出。
- `self_describing`：格式较自描述，例如文本或 map。
- `schema_required`：格式需要外部 schema 才能解释。

调度层会根据 writer 能力检查字段配置。例如 writer 声明 `numeric_fields` 时，
`serialization_field_data::field_id` 必须非 0；writer 声明 `named_fields` 时会使用
`serialized_name` 或 `field_name`。

当前内置后端：

- `text_serialization_writer`：声明 `named_fields | ordered_fields | self_describing`。
- `json_serialization_writer`：声明 `named_fields | ordered_fields | self_describing`，输出紧凑 JSON。

### serialization_reader

`serialization_reader` 是格式读入后端接口。反序列化调度层负责查找 `meta_registry`、匹配反射字段和
写入对象；reader 只负责按字段配置提供当前字段值。

当前内置 reader：

- `variant_serialization_reader`：从 `variant` 对象树读取字段，声明
  `named_fields | numeric_fields | self_describing`。

可以直接从 DOM 中间对象反序列化到已注册反射和序列化元信息的对象：

```cpp
actor instance{};
caors::error status = caors::deserialize(instance, value.value);
```

字段查找优先使用非 0 的 `field_id`，再回退到 `serialized_name` 或 `field_name`。当前支持写入
`bool`、`char`、整数、浮点、`caors::string`、`std::string`、`guid`、`type_guid`、枚举、
已注册反射/序列化元信息的值类型结构体字段、已适配的顺序/集合/关联容器字段、裸指针和普通 wrapper
字段，以及通过 resolver 解析的 `caors::object_ref<T>` 字段。

反序列化缺失字段策略：

- 显式配置字段且 `required == false` 时，缺失字段会被跳过，目标对象保留已有值。
- 显式配置字段且 `required == true` 时，缺失字段返回 `errc::not_found`。
- 未显式配置字段时，反序列化按全部反射字段读入，缺失字段返回 `errc::not_found`。
- DOM 中存在但未被目标类型声明的未知字段默认忽略。

默认值恢复、未知字段保留、严格模式和完整版本迁移策略后续再补。

### JSON

JSON 写出入口：

```cpp
caors::serialization_text_result text = caors::serialize_json(instance);
```

JSON 读入入口先解析为 `variant` DOM，再复用 `variant_serialization_reader` 写回对象：

```cpp
caors::variant_result value = caors::parse_json(text);
caors::error status = caors::deserialize_json(instance, text);
```

当前 JSON parser 是最小实现，支持对象、数组、字符串、整数、浮点、`true`、`false` 和 `null`。
字符串支持常见转义和 ASCII 范围内的 `\u00XX`；非 ASCII `\uXXXX` 暂返回 `errc::unsupported`。
字段 id 无法从 JSON 对象名表达，因此 JSON 读入主要按字段名匹配。

### object references

`caors::object_ref<T>` 是非拥有对象引用。序列化层不会自行管理对象生命周期，也不会假设
`object_id` 可以跨存档稳定使用。调用方需要在 `serialization_context` 中提供
`serialization_reference_resolver`：

```cpp
caors::serialization_reference_resolver resolver{
    &object_to_id,
    &id_to_object,
    user_data,
};
caors::serialization_context context{0, &resolver};
```

写出时，resolver 把 `object*` 转换为 `object_id`，输出为无符号整数；空引用输出 `0`。读入时，
resolver 把非 0 `object_id` 转换回对象指针，再写入 `object_ref<T>` 字段。

失败策略：

- 未提供 resolver 或 resolver 缺少对应方向函数时返回 `errc::invalid_argument`。
- 非空引用写出时无法映射到 id，或读入 id 找不到对象时返回 `errc::not_found`。
- 读入对象不能转换为 `T*` 时返回 `errc::invalid_type`。

### serialization codec

`serialization_codec_registry` 是类型级扩展点。调度层读写非容器普通值时会按类型查询 codec；命中后
由 codec 直接处理该值，未命中时再回退到内置标量探测。这样 `guid`、`type_guid` 这类语义上是
原子值的类型，不需要暴露内部字段结构。容器字段优先通过 meta 容器视图递归处理元素，因此元素也能继续
复用 codec、反射、指针和 wrapper 规则。

内置 codec：

- `caors::string`：按字符串读写，默认走 CAORS allocator。
- `std::string`：按字符串读写，用于兼容外部结构。
- `std::string_view`：按字符串写出和构造 DOM；反序列化暂不支持直接写回 view。
- `caors::guid`：按标准 UUID/GUID 字符串读写。
- `caors::type_guid`：按内部 `guid` 的标准字符串读写。
- `caors::vector<T>` / `std::vector<T>`：通过 `sequence_serialization_codec<T>` 输出为数组。
- `caors::list<T>` / `std::list<T>`：通过 `sequence_serialization_codec<T>` 输出为数组。
- `std::deque<T>`：通过 `sequence_serialization_codec<T>` 输出为数组。
- `std::array<T, N>`：通过 `sequence_serialization_codec<T>` 输出为数组；读入长度必须等于 `N`。
- `caors::hash_set<T>` / `std::unordered_set<T>` / `std::set<T>`：通过 `set_serialization_codec<T>`
  输出为数组，读入时拒绝重复元素。
- `caors::hash_map<K, V>` / `std::unordered_map<K, V>` / `std::map<K, V>`：通过
  `associative_serialization_codec<T>` 输出为字典 pair 数组。

`caors/serialization/stl_codecs.h` 聚合了当前内置容器 codec。`caors/serialization.h` 会继续包含它，
因此既有代码不需要额外 include；如果只想显式引入 STL/CAORS 容器适配，也可以包含
`caors/adapters/stl.h`。

反射字段中的容器不要求显式调用 `register_serialization_codec<T>()`；只要字段类型通过
`sequence_traits<T>`、`set_traits<T>` 或 `associative_traits<T>` 注册了 meta 容器数据，序列化层就能按
元素、key 和 value 递归处理。`sequence_serialization_codec<T>`、`set_serialization_codec<T>` 和
`associative_serialization_codec<T>` 仍保留给显式 codec 注册、独立值转换和外部复用。自定义容器通常只需要
特化 traits；如果还需要作为 codec 单独注册，可以让自己的 `serialization_codec<T>` 继承通用 codec：

```cpp
namespace caors {

template <typename T>
struct serialization_codec<my_sequence<T>> : sequence_serialization_codec<my_sequence<T>> {};

template <typename T>
struct serialization_codec<my_set<T>> : set_serialization_codec<my_set<T>> {};

template <typename K, typename V>
struct serialization_codec<my_map<K, V>> : associative_serialization_codec<my_map<K, V>> {};

}
```

当前 text/json writer 使用 `[[key, value], ...]` 的 pair 数组表示字典，因此可以支持非字符串 key。
DOM reader 同时接受 `variant::dictionary` 和 pair 数组形态。

外部类型可以通过特化 `serialization_codec<T>` 并调用 `register_serialization_codec<T>()` 注册，也可以
直接按 `type_id` 注册 `serialization_codec_data`。codec 可以分别提供 writer、reader 和 DOM
构造函数；没有提供的方向会继续走默认逻辑或返回不支持。

### 指针和 wrapper 字段

反射层把裸指针和普通 wrapper 拆成“外层持有者”和“内部 value”两层。序列化层只处理内部 value：

- `T*`、`std::unique_ptr<T>`、`std::shared_ptr<T>`、`std::optional<T>` 和
  `std::reference_wrapper<T>` 会按内部 `T` 写出。
- 空裸指针、空智能指针或空 `optional` 写出为 `null`；`std::reference_wrapper<T>` 不表示空值。
- 读入 `null` 会重置裸指针、`std::unique_ptr<T>`、`std::shared_ptr<T>` 和 `std::optional<T>`；
  写入 `std::reference_wrapper<T>` 时返回 `errc::invalid_field`。
- 读入非空值时，裸指针要求目标指针已经指向有效对象，不会自动分配内存。
- 读入非空值时，`std::unique_ptr<T>`、`std::shared_ptr<T>` 和 `std::optional<T>` 会在内部为空时尝试默认构造
  `T`；`std::reference_wrapper<T>` 会写入已经绑定的对象。
- `std::weak_ptr<T>` 无法稳定暴露可写内部值，当前不作为普通 value 序列化。
- `caors::object_ref<T>` 不走内部 value 语义，仍通过 `serialization_reference_resolver` 写出和读入对象 id。

这条规则避免序列化层隐式决定所有权。后续如果要让某类 wrapper 采用引用语义、共享身份或自定义构造策略，
应在字段策略或专用 wrapper trait 中显式声明。

## 当前类型处理边界

当前实现优先覆盖可明确表达所有权和生命周期的类型。以下限制是刻意保留的边界，后续应逐项解决：

- `std::string_view` 只支持写出和构造 DOM，不支持反序列化写回，避免把临时字符串读入成悬空引用。
- `std::weak_ptr<T>` 默认不作为 wrapper 展开；如果需要弱引用序列化，应通过对象 handle、资源 id 或
  `object_ref<T>` 类似的显式引用语义处理。
- 裸指针和普通智能指针按 inline value 处理，不保留对象身份、共享关系或对象图引用；需要身份保持时使用
  `caors::object_ref<T>` 或后续对象 handle。
- 类型语义最多支持一层间接类型；`T**`、`std::unique_ptr<T*>`、
  `std::optional<std::shared_ptr<T>>` 等多层组合当前会触发静态断言。
- `std::array<T, N>` 已支持固定长度数组读写；反序列化时数组长度必须等于 `N`，长度不匹配会返回失败。
- `std::span<T>`、`std::pair<T, U>`、`std::tuple<...>`、`std::variant<...>` 尚未内置适配。
  借用视图、异构字段和 tagged union 需要单独设计读写规则。
- 反射对象按字段声明静态类型处理，不会自动记录运行期动态类型；多态序列化需要后续引入类型标识和构造策略。
- JSON reader 目前不完整支持非 ASCII `\uXXXX` 解码，二进制格式、版本迁移和默认值恢复也尚未实现。

### variant

`variant` 是序列化 DOM 中间对象。它保存的是已经脱离 C++ 精确类型的运行期数据，可以保存：

- `null`
- `bool`
- 有符号整数
- 无符号整数
- 浮点数
- 字符串
- 数组
- 字典
- 对象

字典条目由 `variant_entry` 表示，key 和 value 都是 `variant`：

```cpp
struct variant_entry {
  caors::variant key;
  caors::variant value;
};
```

对象成员由 `variant_member` 表示，保留输出字段名、稳定字段 id 和字段值：

```cpp
struct variant_member {
  caors::string_name name;
  std::uint32_t field_id;
  caors::variant value;
};
```

可以从已注册反射和序列化元信息的对象构造中间树：

```cpp
caors::variant_result value = caors::to_serialization_value(instance);
```

也可以把中间树写入任意 writer：

```cpp
caors::text_serialization_writer writer;
caors::write(writer, value.value);
```

DOM 是可选路径，适合工具、编辑器、测试对比和格式转换。性能敏感路径仍可直接使用
`serialize(instance, writer)`，避免构造整棵中间树。

`variant` 提供 `is_*()` 和 `try_as_*()` 安全访问器，避免在类型不匹配时误读默认存储字段。
数组、字典和对象可以通过 `push_back()`、`add_entry()`、`add_member()`、`find_member(name)`
和 `find_member(field_id)` 做基础修改和查询。

运行期 C++ 值可以显式转换为 `variant`：

```cpp
caors::variant_result value = caors::to_variant(caors::any::from(42));
```

`to_variant(any)` 会先查询 serialization codec，再回退到内置标量和字符串转换。`to_variant(any_ref)`
会先尝试复制引用指向的对象；不可拷贝引用会返回 `errc::unsupported`。

### serialization_registry

`serialization_registry` 是全局序列化注册表，内部复用 `meta_registry`：

```cpp
caors::error status = caors::register_serialization<actor>();

if (status) {
  // 处理重复注册、缺失类型等错误。
}
```

重复注册同一个类型会返回 `errc::duplicate_component`，错误来源域为
`error_domain::serialization`。

### serialize_text

`serialize_text()` 使用已注册的反射字段读取对象：

```cpp
caors::reflection::class_<actor>("game.actor")
    .field("id", &actor::id)
    .field("health", &actor::health)
    .commit();

caors::register_serialization<actor>();

actor instance{7, 95.5F};
caors::serialization_text_result result = caors::serialize_text(instance);
caors::string text = result.value;
```

输出格式是 MVP 内部简单文本格式，例如：

```text
{"id":7,"health":95.5}
```

当前支持的字段值：

- `bool`
- 字符和整数类型
- 浮点类型
- `caors::string`
- `std::string`
- `std::string_view`
- 已注册 codec 的容器类型
- 已注册反射和序列化元信息的嵌套对象
- 裸指针、普通 wrapper 字段和 `caors::object_ref<T>`

遇到不可拷贝字段、未支持字段类型、无 resolver 的对象引用或超出当前类型边界的字段时返回错误。

## 当前已实现能力

- `serialization_type_data`：序列化组件数据。
- `serialization_field_data`：字段级序列化配置。
- `serialization_writer`：格式后端接口。
- `serialization_reader`：格式读入后端接口。
- `serialization_context`：单次序列化/反序列化上下文，支持 schema version 字段过滤。
- `serialization_codec_registry`：类型级 codec 注册表。
- `text_serialization_writer`：简单文本格式后端。
- `variant`：序列化 DOM 中间对象。
- `to_variant()`：把运行期 `any` / `any_ref` 转换为 `variant`。
- `variant_serialization_reader`：从 DOM 中间对象读入字段的后端。
- `variant_member`：对象成员，保存字段名、字段 id 和字段值。
- `variant_entry`：字典条目，保存 key 和 value。
- `serialization_registry`：全局序列化注册表。
- `register_serialization()`：注册序列化组件。
- `serialize()`：通过指定 writer 后端序列化对象。
- `deserialize()`：通过指定 reader 后端反序列化对象。
- `to_serialization_value()`：通过反射和序列化元信息构造中间对象树。
- `write()`：把中间对象树写入指定 writer。
- `serialization_text_result`：`caors::result<caors::string>` 文本序列化结果。
- `variant_result`：`caors::result<variant>` 中间值构造结果。
- `serialize_text()`：基于反射字段的简单文本序列化。
- 基础标量和字符串字段输出。
- 字段别名和跳过配置。
- 反序列化 required/optional 缺失字段策略。
- DOM 反序列化默认忽略未知字段。
- 基础 schema version 字段过滤。
- 内置 string、string_view、guid 和 type_guid codec。
- 裸指针、普通 wrapper、`object_ref<T>` 和已适配容器字段。
- `variant` 安全访问器、数组/字典/对象辅助修改和成员查询。
- 序列化注册早于反射字段注册。

## 相关入口

- 公共聚合头：`include/caors/serialization.h`。
- 按职责拆分的公共头位于 `include/caors/serialization/`。
- 行为测试：`tests/serialization/`。
