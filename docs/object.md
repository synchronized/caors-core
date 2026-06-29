<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Object

本文档说明 CAORS 对象系统的当前设计边界、已实现能力和后续扩展方向。当前对象系统处于基础阶段，目标是为后续反射、序列化、资源系统和编辑器工具提供稳定入口。

## 设计目标

CAORS 对象系统第一阶段只解决几件事：

- 对象实例有运行期唯一标识。
- 对象类型复用基础 `meta_type_data`，包含类型标识、名称、稳定 GUID、大小、对齐和父类型。
- 对象可以注册到基于 `meta_registry` 的对象注册表。
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

### meta_type_data

`meta_type_data` 是对象系统和后续反射系统共享的基础类型描述，包含：

- `id`
- `name`
- 可选 `type_guid`
- `size`
- `alignment`
- `parent_id`

`object_type_id` 是 `type_id` 在对象系统里的别名。它由全局 `meta_registry` 分配，不由对象
类型名派生：

```cpp
caors::object_type_id id = player::static_type_id();
```

对象类型名用于查找、调试和工具展示；稳定序列化身份应由显式 `type_guid`、上层格式和迁移策略
处理。

### object

`object` 是可选基类，提供：

- `id()`
- `type_id()`
- `type_name()`

它不负责对象所有权，也不内置引用计数或 GC。

### object_base

`object_base<Derived, Base>` 帮助派生类提供运行期类型查询。派生类需要提供 `object_type_name`：

```cpp
class actor : public caors::object_base<actor> {
public:
  static constexpr std::string_view object_type_name = "game.actor";
};

class player : public caors::object_base<player, actor> {
public:
  static constexpr std::string_view object_type_name = "game.player";
  static constexpr caors::type_guid object_type_guid{caors::guid::from_bytes(
      caors::guid::byte_array{0x92, 0x8d, 0x35, 0x70, 0x54, 0x68, 0x46, 0x5c,
                              0x9d, 0xc4, 0x30, 0x7f, 0xb7, 0x45, 0xf2, 0x2c})};
};
```

`object_type_guid` 是可选静态成员。运行期临时对象可以不声明，`meta_type_data::guid()` 会保持 nil；可序列化、可反射或跨插件暴露的对象类型建议显式声明稳定 GUID。

`static_type_id()` 会确保 C++ 类型在全局元数据注册表中拥有一个运行期 id。对象类型尚未正式注册
时，该记录可以是匿名的；调用 `register_object_type<T>()` 后会补充类型名、GUID 和父类型关系。

这样通过基类引用也能查询真实类型：

```cpp
caors::object& object = player_instance;
object.type_id();   // player::static_type_id()
object.type_name(); // "game.player"
```

### object_registry

`object_registry` 是基于全局 `meta_registry` 的对象注册表和默认构造工厂。它通过全局 `meta_registry` 管理类型 id、名称和父类型关系，并将 `object_type` 作为对象元数据组件挂载到同一个 `type_id` 上。

```cpp
caors::object_registry registry;

if (!caors::object_register_succeeded(
      caors::register_object_type<actor>(registry, actor::object_type_name))) {
  // 处理注册失败。
}

caors::object_register_result result =
    caors::register_object_type<player>(registry, player::object_type_name);
```

注册后可以查询类型：

```cpp
auto type = registry.find_type<player>();
```

也可以检查继承关系：

```cpp
registry.is_a(player::static_type_id(), actor::static_type_id());
```

注册失败会返回明确的 `object_register_result`：

- `success`：注册成功。
- `invalid_type`：类型描述无效。
- `duplicate_id`：同一个运行期 id、C++ 类型或对象元数据组件已注册。
- `duplicate_name`：同一个名称已注册。
- `duplicate_guid`：同一个非 nil 稳定 GUID 已注册。
- `missing_parent`：父类型未注册。

## 创建和销毁

注册表支持按 `object_type_id` 创建默认构造对象：

```cpp
caors::object* instance = registry.create(player::static_type_id());
registry.destroy(instance);
```

也可以直接使用 C++ 类型创建已注册对象：

```cpp
player* instance = registry.create<player>();
registry.destroy(instance);
```

也可以显式传入内存资源：

```cpp
caors::tracking_resource tracking(&caors::get_system_memory_resource());

player* instance = registry.create<player>(&tracking);
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
- `meta_type_data` / `meta_registry`：对象系统和反射系统共享的全局基础类型描述和元数据注册表。
- `object_type`：基于 `meta_type_data` 扩展对象创建和销毁能力。
- `object_register_result`：明确对象类型注册失败原因。
- `object_registry`：类型注册、查询、继承关系判断、默认构造工厂。
- `get_object_registry()`：进程内默认对象注册表。
- `create_object<T>()` / `destroy_object<T>()`：接入 `memory_resource` 的直接对象创建和销毁入口。

## 后续路线图

- [x] 对象实例 id。
- [x] 对象类型 id、名称、大小、对齐和父类型。
- [x] 复用全局 `meta_registry`。
- [x] 将 `object_type` 作为类型元数据组件挂载。
- [x] 注册结果枚举和不一致类型描述检测。
- [x] 默认构造工厂。
- [x] 接入 `memory_resource`。
- [ ] 明确对象 handle / weak handle 设计。
- [ ] 增加对象生命周期管理器。
- [ ] 与反射系统打通字段、方法和属性元信息。
- [ ] 与序列化系统打通类型名、版本和构造流程。
- [ ] 评估是否需要线程安全对象容器或对象数据库。

## 相关入口

- 可运行示例位于 `examples/object/`，示例组织规则见 [examples/README.md](../examples/README.md)。
- 行为测试位于 `tests/object/`，测试组织规则见 [tests/README.md](../tests/README.md)。
