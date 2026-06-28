<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Object

本文档说明 CAORS 对象系统的当前设计边界、已实现能力和后续扩展方向。当前对象系统处于基础阶段，目标是为后续反射、序列化、资源系统和编辑器工具提供稳定入口。

## 设计目标

CAORS 对象系统第一阶段只解决几件事：

- 对象实例有运行期唯一标识。
- 对象类型复用基础 `type_info`，包含类型标识、名称、大小、对齐和父类型。
- 对象可以注册到基于 `type_registry` 的对象注册表。
- 注册表可以按类型创建和销毁默认构造对象。
- 对象创建和销毁可以接入 `memory_resource`。

当前不会实现：

- GC 或引用计数。
- 脚本绑定。
- 信号事件系统。
- 属性字段反射。
- 序列化细节。

这些能力后续应建立在对象类型信息和反射系统之上。

## 核心概念

### object_id

`object_id` 是运行期对象实例标识。`0` 保留为无效值。

```cpp
actor instance;
caors::object_id id = instance.id();
```

它只表示当前进程运行期间的对象身份，不承诺跨进程、跨版本或跨存档稳定。

### type_info

`type_info` 是对象系统和后续反射系统共享的基础类型描述，包含：

- `id`
- `name`
- `size`
- `alignment`
- `parent_id`

`object_type_id` 是 `type_id` 在对象系统里的别名。类型标识由类型名哈希得到：

```cpp
caors::object_type_id id = caors::make_object_type_id("game.player");
```

类型名应由使用方保持稳定。后续序列化版本兼容仍应由上层格式和迁移策略处理。

### object

`object` 是可选基类，提供：

- `id()`
- `type_id()`
- `type_name()`

它不负责对象所有权，也不内置引用计数或 GC。

### object_base

`object_base<Derived, Base>` 帮助派生类实现运行期类型查询。派生类需要提供 `object_type_name`：

```cpp
class actor : public caors::object_base<actor> {
public:
  static constexpr std::string_view object_type_name = "game.actor";
};

class player : public caors::object_base<player, actor> {
public:
  static constexpr std::string_view object_type_name = "game.player";
};
```

这样通过基类引用也能查询真实类型：

```cpp
caors::object& object = player_instance;
object.type_name(); // "game.player"
```

### object_registry

`object_registry` 是基于 `type_registry` 的对象注册表和默认构造工厂。它复用 `type_registry` 管理类型 id、名称和父类型关系，自己只额外保存对象创建和销毁函数。

```cpp
caors::object_registry registry;

if (!caors::object_register_succeeded(
      caors::register_object_type<actor>(registry, actor::object_type_name))) {
  // 处理注册失败。
}

caors::object_register_result result =
    caors::register_object_type<player>(registry, player::object_type_name,
                                        actor::static_type_id());
```

注册后可以查询类型：

```cpp
auto type = registry.find_type(player::static_type_id());
```

也可以检查继承关系：

```cpp
registry.is_a(player::static_type_id(), actor::static_type_id());
```

注册失败会返回明确的 `object_register_result`：

- `success`：注册成功。
- `invalid_type`：类型描述无效。
- `duplicate_id`：同一个 id 已注册。
- `duplicate_name`：同一个名称已注册。
- `hash_collision`：id 已存在，但名称不同，说明发生 hash 碰撞或手写 id 错误。
- `missing_parent`：父类型未注册。

## 创建和销毁

注册表支持按 `object_type_id` 创建默认构造对象：

```cpp
caors::object* instance = registry.create(player::static_type_id());
registry.destroy(instance);
```

也可以显式传入内存资源：

```cpp
caors::tracking_resource tracking(&caors::get_system_memory_resource());

caors::object* instance = registry.create(player::static_type_id(), &tracking);
registry.destroy(instance, &tracking);
```

`registry.destroy()` 需要传回与创建时一致的内存资源。当前对象本身不保存分配来源。

对于需要构造参数的类型，可以直接使用模板入口：

```cpp
auto* instance = caors::create_object<player>(&resource, constructor_arg);
caors::destroy_object(instance, &resource);
```

非默认构造类型仍可注册类型描述，但不能通过注册表默认创建。

## 当前已实现能力

- `object_id`：运行期对象实例标识。
- `object_type_id`：运行期对象类型标识。
- `object`：基础对象身份和类型查询入口。
- `object_base<Derived, Base>`：CRTP 类型查询辅助。
- `type_info` / `type_registry`：对象系统和反射系统共享的基础类型描述和注册表。
- `object_type`：基于 `type_info` 扩展对象创建和销毁能力。
- `object_register_result`：明确对象类型注册失败原因。
- `object_registry`：类型注册、查询、继承关系判断、默认构造工厂。
- `get_object_registry()`：进程内默认对象注册表。
- `create_object<T>()` / `destroy_object<T>()`：接入 `memory_resource` 的直接对象创建和销毁入口。

## 后续路线图

- [x] 对象实例 id。
- [x] 对象类型 id、名称、大小、对齐和父类型。
- [x] 复用共享 `type_registry`。
- [x] 注册结果枚举和 hash 碰撞检测。
- [x] 默认构造工厂。
- [x] 接入 `memory_resource`。
- [ ] 明确对象 handle / weak handle 设计。
- [ ] 增加对象生命周期管理器。
- [ ] 与反射系统打通字段、方法和属性元信息。
- [ ] 与序列化系统打通类型名、版本和构造流程。
- [ ] 评估是否需要线程安全对象容器或对象数据库。

## 示例索引

可运行示例位于：

- `examples/object/basic_object.cpp`

行为测试位于：

- `tests/object/object_test.cpp`
