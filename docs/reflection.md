<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Reflection

本文档说明 CAORS 反射系统的当前设计边界。第一版反射只提供类型和字段元信息注册、查询，不提供动态 get/set、方法调用、属性系统或代码生成。

## 设计目标

反射第一阶段解决几件事：

- 复用 `type_info`、`type_id` 和 `type_guid` 表达类型身份。
- 基于 `meta_registry` 把 `reflection_type` 挂载为类型元数据组件。
- 通过 RTTR 风格 builder 或底层字段列表描述字段。
- 提供按类型 id、类型名和稳定 GUID 查询反射类型。
- 提供按字段名查询字段元信息。

当前不会包含：

- 动态字段读写。
- 方法调用。
- 属性元数据。
- 自动代码扫描或代码生成。
- 容器、枚举和复杂类型转换规则。

## 核心概念

### field_info

`field_info` 描述一个字段：

- `name`：字段名。
- `type`：字段类型的运行期 `type_id`。
- `type_name`：字段类型名。
- `offset`：字段在类型中的偏移。
- `size`：字段大小。
- `alignment`：字段对齐。

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
- 如果字段类型没有注册，但存在 `type_name<T>` 默认映射，则使用默认类型名生成字段类型 id。
- 如果两者都没有，`.commit()` 返回 `invalid_field`。

### reflection_type

`reflection_type` 是类型反射描述，包含基础 `type_info` 和字段列表：

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

### reflection_registry

`reflection_registry` 复用 `meta_registry` 保存基础类型信息，并把 `reflection_type` 作为组件挂载到同一个 `type_id` 上：

```cpp
caors::reflection_registry registry;

caors::reflection_register_result result =
    caors::reflection::class_<actor>(registry, "game.actor")
        .field("id", &actor::id)
        .commit();
```

注册失败会返回明确的 `reflection_register_result`：

- `success`：注册成功。
- `invalid_type`：类型描述无效。
- `invalid_field`：字段描述无效。
- `duplicate_id`：同一个 id 已注册。
- `duplicate_name`：同一个名称已注册。
- `duplicate_guid`：同一个非 nil 稳定 GUID 已注册。
- `duplicate_field`：同一个类型内存在重复字段名。
- `duplicate_reflection`：目标类型已经挂载反射组件。
- `inconsistent_type`：目标类型已存在，但基础 `type_info` 不一致。
- `hash_collision`：id 已存在但名称不同。
- `missing_parent`：父类型未注册。

## 当前已实现能力

- `field_info`：字段名、字段类型名、offset、size 和 alignment。
- `reflection_type`：基础类型描述和字段列表。
- `reflection_registry`：反射类型注册、查询、清理和统计。
- `reflection::class_<T>()`：RTTR 风格链式注册入口。
- `register_reflection<T>()`：底层字段列表注册入口。
- `find_reflection()`：在默认反射注册表中查询类型。

## 相关入口

- 可运行示例位于 `examples/reflection/`，示例组织规则见 [examples/README.md](../examples/README.md)。
- 行为测试位于 `tests/reflection/`，测试组织规则见 [tests/README.md](../tests/README.md)。
