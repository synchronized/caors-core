<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Serialization

本文档说明 CAORS 序列化层的当前 MVP。当前实现只提供基于反射字段的简单文本序列化，用于验证
`meta_registry`、反射字段访问和统一错误模型之间的协作边界。

## 设计目标

第一阶段解决几件事：

- 用 `serialization_type_data` 表示类型的序列化元信息。
- 用 `serialization_field_data` 表示字段级序列化配置，例如字段别名、跳过和版本信息。
- 基于全局 `meta_registry` 挂载序列化组件，避免单独维护类型索引。
- 提供 `serialization_registry` 和 `register_serialization<T>()` 注册入口。
- 提供 `serialize_text()`，通过反射字段读取对象并输出简单文本结构。
- 用 `caors::error` 和 `serialization_text_result` 表达失败状态。

当前不会包含：

- 反序列化。
- 二进制格式。
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
- `version_added` / `version_removed`：预留给版本迁移。
- `required`：预留给反序列化缺失字段检查。
- `skipped`：序列化时跳过该字段。

序列化字段配置属于 serialization 组件，不直接写入 `reflection_field_data`，也不持有
`reflection_field_data` 指针。反射负责描述字段是什么，序列化负责描述字段如何存储；真正序列化时
再按字段名解析反射字段，因此序列化注册和反射注册没有固定顺序要求。

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
- `serialization_registry`：全局序列化注册表。
- `register_serialization()`：注册序列化组件。
- `serialization_text_result`：文本序列化结果。
- `serialize_text()`：基于反射字段的简单文本序列化。
- 基础标量和字符串字段输出。
- 字段别名和跳过配置。
- 序列化注册早于反射字段注册。

## 相关入口

- 公共头文件：`include/caors/serialization.h`。
- 行为测试：`tests/serialization/`。
