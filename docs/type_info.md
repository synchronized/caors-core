<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Type Info

本文档说明 CAORS 基础类型信息层的当前设计。完整的类型元数据设计见 [meta.md](meta.md)。`caors/type_info.h` 当前作为兼容头转发到 `caors/meta.h`。

## 设计目标

`type_info` 第一阶段解决几件事：

- 提供运行期类型标识。
- 使用 `string_name` 保存运行期去重后的类型名。
- 保存类型名、大小、对齐和父类型。
- 提供基础类型注册、查询和继承关系判断。
- 让对象系统和反射系统共享同一套基础类型索引。

当前不会包含：

- 对象创建和销毁函数。
- 字段、方法、属性或枚举项元信息。
- 序列化版本迁移策略。

这些能力应由对象系统、反射系统和序列化系统通过 `meta_registry` 扩展。

## 核心概念

### string_name

`string_name` 是运行期字符串名。相同文本共享同一个运行期 id，适合快速比较和查找：

```cpp
caors::string_name name{"game.player"};
std::string_view view = name.view();
```

`string_name::id()` 只保证当前进程内唯一，不承诺跨进程或跨版本稳定。

### type_id

`type_id` 是运行期类型标识。`0` 保留为无效值。

```cpp
caors::type_id id = caors::make_type_id("game.player");
```

当前 `make_type_id()` 会先把类型名转为 `string_name`，再用 `string_name::id()` 派生 `type_id`。它适合运行期查找，不适合作为序列化长期稳定 ID；后续序列化应增加显式 `type_guid`。

### type_info

`type_info` 是基础类型描述，包含：

- `id`
- `name`，类型为 `string_name`
- `size`
- `alignment`
- `parent_id`

可以通过模板辅助函数创建：

```cpp
caors::type_info info = caors::make_type_info<player>("game.player", actor_id);
```

### meta_registry

`meta_registry` 负责注册和查询基础类型：

```cpp
caors::meta_registry registry;

caors::type_register_result result =
    caors::register_type<actor>(registry, "game.actor");
```

注册表拥有类型名存储，因此 `find_type()` 返回的 `type_info::name` 在注册表生命周期内有效。

注册失败会返回明确的 `type_register_result`：

- `success`：注册成功。
- `invalid_type`：类型描述无效。
- `duplicate_id`：同一个 id 已注册。
- `duplicate_name`：同一个名称已注册。
- `hash_collision`：id 已存在，但名称不同，说明手写 id 错误或外部构造了不一致的类型描述。
- `missing_parent`：父类型未注册。

## 与 meta 系统的关系

`type_info` 是 `meta_registry` 的基础类型组件：

- `meta_registry` 直接管理 id、名称和父类型。
- `object_registry` 通过 `meta_registry` 挂载 `object_type` 组件。
- 后续 `reflection_registry` 应通过 `meta_registry` 挂载 `reflection_type` 组件。

这样对象注册和反射注册不会重复实现重名、重复 id、不一致类型描述和父类型检查。

## 当前已实现能力

- `type_id`：运行期类型标识。
- `string_name`：运行期 interned 名称。
- `make_type_id()`：根据类型名生成类型标识。
- `type_info`：基础类型描述。
- `make_type_info<T>()`：根据 C++ 类型创建基础类型描述。
- `type_register_result`：明确基础类型注册失败原因。
- `meta_registry`：基础类型注册、查询、继承关系判断、清理和组件式类型元数据注册表，见 [meta.md](meta.md)。

行为测试位于：

- `tests/type/type_info_test.cpp`
