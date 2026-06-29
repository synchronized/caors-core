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
- 提供 `serialization_value` 中间对象树，方便工具、调试和多格式转换。
- 提供 `text_serialization_writer` 和 `serialize_text()`，通过反射字段读取对象并输出简单文本结构。
- 提供 `value_serialization_reader` 和 `deserialize()`，通过 DOM 中间对象写回基础反射字段。
- 用 `caors::error` 和 `serialization_text_result` 表达失败状态。

当前不会包含：

- 内置二进制格式。
- 版本迁移和缺失字段恢复。
- 对象引用、指针和 wrapper 字段序列化。
- 容器和嵌套结构递归序列化。

这些能力后续应在对象 handle、字段策略和格式扩展点明确后逐步补充。

## 核心概念

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
- `version_added` / `version_removed`：预留给版本迁移。
- `required`：预留给反序列化缺失字段检查。
- `skipped`：序列化时跳过该字段。

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

### serialization_reader

`serialization_reader` 是格式读入后端接口。反序列化调度层负责查找 `meta_registry`、匹配反射字段和
写入对象；reader 只负责按字段配置提供当前字段值。

当前内置 reader：

- `value_serialization_reader`：从 `serialization_value` 对象树读取字段，声明
  `named_fields | numeric_fields | self_describing`。

可以直接从 DOM 中间对象反序列化到已注册反射和序列化元信息的对象：

```cpp
actor instance{};
caors::error status = caors::deserialize(instance, value.value);
```

字段查找优先使用非 0 的 `field_id`，再回退到 `serialized_name` 或 `field_name`。当前只支持写入
`bool`、整数、浮点和 `std::string` 字段；缺失字段会返回 `errc::not_found`。`required`、默认值、
未知字段保留和版本迁移策略后续再补。

### serialization_value

`serialization_value` 是序列化 DOM 中间对象。它可以保存：

- `null`
- `bool`
- 有符号整数
- 无符号整数
- 浮点数
- 字符串
- 数组
- 对象

对象成员由 `serialization_member` 表示，保留输出字段名、稳定字段 id 和字段值：

```cpp
struct serialization_member {
  caors::string_name name;
  std::uint32_t field_id;
  caors::serialization_value value;
};
```

可以从已注册反射和序列化元信息的对象构造中间树：

```cpp
caors::serialization_value_result value = caors::to_serialization_value(instance);
```

也可以把中间树写入任意 writer：

```cpp
caors::text_serialization_writer writer;
caors::write(writer, value.value);
```

DOM 是可选路径，适合工具、编辑器、测试对比和格式转换。性能敏感路径仍可直接使用
`serialize(instance, writer)`，避免构造整棵中间树。

### serialization_registry

`serialization_registry` 是全局序列化注册表，内部复用 `meta_registry`：

```cpp
caors::error status = caors::register_serialization<actor>();

if (status.failed()) {
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
```

输出格式是 MVP 内部简单文本格式，例如：

```text
{"id":7,"health":95.5}
```

当前支持的字段值：

- `bool`
- 字符和整数类型
- 浮点类型
- `std::string`
- `std::string_view`

遇到裸指针、wrapper、不可拷贝字段或未支持的字段类型时返回 `errc::unsupported`。

## 当前已实现能力

- `serialization_type_data`：序列化组件数据。
- `serialization_field_data`：字段级序列化配置。
- `serialization_writer`：格式后端接口。
- `serialization_reader`：格式读入后端接口。
- `text_serialization_writer`：简单文本格式后端。
- `serialization_value`：序列化 DOM 中间对象。
- `value_serialization_reader`：从 DOM 中间对象读入字段的后端。
- `serialization_member`：对象成员，保存字段名、字段 id 和字段值。
- `serialization_registry`：全局序列化注册表。
- `register_serialization()`：注册序列化组件。
- `serialize()`：通过指定 writer 后端序列化对象。
- `deserialize()`：通过指定 reader 后端反序列化对象。
- `to_serialization_value()`：通过反射和序列化元信息构造中间对象树。
- `write()`：把中间对象树写入指定 writer。
- `serialization_text_result`：文本序列化结果。
- `serialize_text()`：基于反射字段的简单文本序列化。
- 基础标量和字符串字段输出。
- 字段别名和跳过配置。
- 序列化注册早于反射字段注册。

## 相关入口

- 公共头文件：`include/caors/serialization.h`。
- 行为测试：`tests/serialization/`。
