<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Meta

本文档说明 CAORS 类型元数据层的当前设计。`meta` 不是运行期游戏 ECS，而是围绕 `type_id` 组织类型、对象、反射和序列化元信息的共享底座。

## 设计目标

`meta` 第一阶段解决几件事：

- 用 `type_id` 表示稳定的运行期类型 key。
- 用 `type_info` 保存类型名、大小、对齐和父类型。
- 用 `meta_registry` 集中处理基础类型注册、查找和继承关系判断。
- 用 `meta_registry` 在同一个 `type_id` 上挂载不同主题的元数据组件。
- 避免对象系统、反射系统和序列化系统各自重复实现类型注册逻辑。

当前不会包含：

- 运行期实体 ECS。
- system 调度。
- 对象生命周期管理器。
- 具体字段、方法或序列化格式。

这些能力应由对象、反射和序列化模块基于 `meta` 扩展。

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

```cpp
caors::type_info info = caors::make_type_info<player>("game.player", actor_id);
```

### meta_registry

`meta_registry` 是类型元数据注册表。它直接管理基础类型信息，并允许在同一个 `type_id` 上挂载对象、反射、序列化等元数据组件：

```cpp
caors::meta_registry registry;

caors::type_register_result result =
    caors::register_type<actor>(registry, "game.actor");
```

注册失败会返回明确的 `type_register_result`：

- `success`：注册成功。
- `invalid_type`：类型描述无效。
- `duplicate_id`：同一个 id 已注册。
- `duplicate_name`：同一个名称已注册。
- `hash_collision`：id 已存在，但名称不同，说明发生 hash 碰撞或手写 id 错误。
- `missing_parent`：父类型未注册。

使用方式是先注册基础类型，再给该类型挂载组件：

```cpp
struct object_factory_meta {
  bool constructible = false;
};

caors::meta_registry registry;
caors::register_type<actor>(registry, "game.actor");

caors::type_id actor_id = caors::make_type_id("game.actor");
registry.emplace<object_factory_meta>(actor_id, true);
```

组件 API：

- `emplace<T>(type_id, args...)`：给已注册类型挂载组件。
- `has<T>(type_id)`：判断类型是否拥有组件。
- `find<T>(type_id)`：获取组件副本。
- `remove<T>(type_id)`：移除组件。
- `view<T...>()`：查询同时拥有指定组件集合的类型。

`meta_component_result` 用于描述组件挂载失败原因：

- `success`：挂载成功。
- `invalid_type`：传入无效 `type_id`。
- `missing_type`：目标类型尚未注册。
- `duplicate_component`：目标类型已经挂载同类组件。

## 与对象和反射的关系

`meta_registry` 是共享元数据层：

- `object_registry` 先注册 `type_info`，再给同一个 `type_id` 挂载 `object_type` 组件。
- 后续 `reflection_registry` 应先复用基础类型信息，再挂载 `reflection_type` 组件。
- 后续 `serialization_registry` 可以挂载序列化相关组件。

这样 `object_register`、`reflection_register` 和 `serialization_register` 不需要重复处理重名、重复 id、hash 碰撞和父类型缺失。

## 当前已实现能力

- `type_id`：运行期类型标识。
- `make_type_id()`：根据类型名生成类型标识。
- `type_info` / `make_type_info<T>()`：基础类型描述。
- `meta_registry`：基础类型注册、查询、继承关系判断、清理和组件式元数据挂载。
- `meta_view<T...>`：按组件集合查询类型。
- `get_meta_registry()`：进程内默认元数据注册表。

行为测试位于：

- `tests/type/type_info_test.cpp`
- `tests/meta/meta_test.cpp`
