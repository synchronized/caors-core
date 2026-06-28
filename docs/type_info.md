<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Type Info

本文档说明 CAORS 基础类型信息层的当前设计。该层只负责类型身份、名称、布局和父类型关系，供对象系统、反射系统和后续序列化系统复用。

## 设计目标

`type_info` 第一阶段解决几件事：

- 提供稳定的运行期类型标识。
- 保存类型名、大小、对齐和父类型。
- 提供基础类型注册、查询和继承关系判断。
- 让对象系统和反射系统共享同一套基础类型索引。

当前不会包含：

- 对象创建和销毁函数。
- 字段、方法、属性或枚举项元信息。
- 序列化版本迁移策略。

这些能力应由对象系统、反射系统和序列化系统在 `type_info` 之上扩展。

## 核心概念

### type_id

`type_id` 是运行期类型标识。`0` 保留为无效值。

```cpp
caors::type_id id = caors::make_type_id("game.player");
```

当前 `make_type_id()` 使用类型名生成 64 位哈希。类型名应由使用方保持稳定；如果用于存档兼容，上层仍需要额外的版本和迁移策略。

### type_info

`type_info` 是基础类型描述，包含：

- `id`
- `name`
- `size`
- `alignment`
- `parent_id`

可以通过模板辅助函数创建：

```cpp
caors::type_info info = caors::make_type_info<player>("game.player", actor_id);
```

### type_registry

`type_registry` 负责注册和查询基础类型：

```cpp
caors::type_registry registry;

caors::type_register_result result =
    caors::register_type<actor>(registry, "game.actor");
```

注册表拥有类型名存储，因此 `find_type()` 返回的 `type_info::name` 在注册表生命周期内有效。

注册失败会返回明确的 `type_register_result`：

- `success`：注册成功。
- `invalid_type`：类型描述无效。
- `duplicate_id`：同一个 id 已注册。
- `duplicate_name`：同一个名称已注册。
- `hash_collision`：id 已存在，但名称不同，说明发生 hash 碰撞或手写 id 错误。
- `missing_parent`：父类型未注册。

## 与对象系统和反射系统的关系

`type_registry` 是共享基础层：

- `object_registry` 复用 `type_registry` 管理 id、名称和父类型，同时额外保存对象创建和销毁函数。
- 后续 `reflection_registry` 应复用 `type_registry` 管理基础类型身份，同时额外保存字段、方法、属性和枚举元信息。

这样对象注册和反射注册不会重复实现哈希碰撞检查、重名检查和父类型检查。

## 当前已实现能力

- `type_id`：运行期类型标识。
- `make_type_id()`：根据类型名生成类型标识。
- `type_info`：基础类型描述。
- `make_type_info<T>()`：根据 C++ 类型创建基础类型描述。
- `type_register_result`：明确基础类型注册失败原因。
- `type_registry`：基础类型注册、查询、继承关系判断和清理。
- `get_type_registry()`：进程内默认基础类型注册表。

行为测试位于：

- `tests/type/type_info_test.cpp`
