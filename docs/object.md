<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Object

本文档说明 CAORS 对象系统的当前设计边界、已实现能力和后续扩展方向。当前对象系统处于基础阶段，目标是为后续反射、序列化、资源系统和编辑器工具提供稳定入口。

## 设计目标

CAORS 对象系统第一阶段只解决几件事：

- 对象实例有运行期唯一标识。
- 对象类型复用 `meta::type`，包含类型标识、名称、稳定 GUID、大小、对齐和父类型。
- 对象可以注册到基于 `meta::registry` 的对象注册表。
- 注册表可以按类型创建和销毁默认构造对象。
- 对象创建和销毁可以接入 `memory_resource`。
- 对象生命周期管理器可以持有实例、按 `object_id` 查找对象，并解析弱句柄和序列化引用。

当前不会实现：

- GC。
- manager 感知的引用计数回收策略。
- 脚本绑定。
- 信号事件系统。
- 属性字段反射。
- 序列化细节。

这些能力后续应建立在对象类型信息和反射系统之上。

## 核心概念

### 命名空间

对象系统的主定义位于 `caors::objects` 命名空间，例如 `objects::object`、`objects::object_id`、
`objects::object_base<T>`、`objects::type`、`objects::object_handle`、
`objects::typed_object_handle<T>`、`objects::object_ptr<T>`、`objects::ref_counted`、`objects::strong_ref<T>`、
`objects::weak_ref<T>`、`objects::registry`、`objects::object_index` 和 `objects::manager`。

为了保持常用 API 简洁，根命名空间保留兼容别名：`caors::object`、`caors::object_id`、
`caors::object_type_id`、`caors::object_base<T>`、`caors::object_ptr<T>` 和
`caors::object_manager`。新代码在模块内部实现和低频辅助类型上优先使用 `caors::objects::*`。

### object_id

`object_id` 是运行期对象实例标识。`0` 保留为无效值。普通对象构造时会通过
`objects::generate_object_id()` 从 CAORS 核心库中的全局原子生成器取得新 id；`objects::manager`
只负责持有和索引对象，不负责生成 id。

```cpp
actor instance;
caors::object_id id = instance.id();
```

正常情况下 id 在当前进程内全局递增且不复用；理论上耗尽 `std::uint64_t` 可用空间后会返回无效 id，
不会回绕复用旧 id。它只表示当前进程运行期间的对象身份，不承诺跨进程、跨版本或跨存档稳定。需要
持久对象引用时，后续应使用显式 `object_guid` / 业务持久 id，再在加载时映射到运行期 `object_id`。
`generate_object_id()` 内部使用原子计数器，可以从多个线程并发调用。

### object_guid 和 object_index

`object_guid` 是对象系统里对 `core::guid` 的语义别名，用于表达跨存档、资源、网络或场景文件稳定的
对象身份。CAORS 不自动生成持久 GUID；调用方应从资源数据库、场景文件或编辑器保存的数据中提供。

`objects::object_index` 保存 `object_guid <-> object_id` 的双向映射，但不拥有对象：

```cpp
caors::objects::object_index index;

index.bind(persistent_guid, instance.id());
caors::object_id runtime_id = index.find(persistent_guid);
caors::object_guid guid = index.guid_of(runtime_id);
```

同一个 GUID 和 id 重复绑定到同一对值会视为成功；同一个 GUID 或 id 试图绑定到另一个值会失败。
nil GUID 和无效 `object_id` 不能绑定。索引不提供内部同步，跨线程访问时调用方需要自行加锁。

### 线程安全边界

对象系统当前只保证 `objects::generate_object_id()` 的并发调用安全。它只负责生成当前进程内唯一的
运行期 id，不访问对象实例、注册表或 manager。

其他对象系统组件默认不提供内部同步：

- `objects::manager`：创建、销毁、查找、遍历和 `object_index` 联动都需要调用方保证外部同步。
- `objects::object_index`：绑定、解绑和查询需要和对应对象生命周期保持一致，跨线程访问时由调用方加锁。
- `objects::object_ptr<T>`、`objects::object_handle` 和 `objects::typed_object_handle<T>`：只是轻量引用或身份描述，不延长对象生命周期，也不隐式保证对象仍然存活。
- 对象类型注册应在启动注册阶段完成；运行期并发注册和查询的策略由上层调度保证。

因此对象系统不会维护全局对象实例表，也不会在 `object_ptr` 或 `object_handle` 中自动解引用全局状态。需要跨线程共享对象时，
建议在上层 manager、场景数据库或资源系统外包一层同步和生命周期策略。

### 基础类型信息

基础类型信息由全局 `meta::registry` 内部保存，并通过 `meta::type` 对外查询，包含：

- `id`
- `name`
- 可选 `type_guid`
- `size`
- `alignment`
- `parent_id`

其中名称、GUID 和父类型属于基础类型数据；大小、对齐和默认值操作函数来自同一 `type_id`
挂载的内部 `meta::detail::type_descriptor`。对象模块只通过 `meta::type` 查询这些信息，不直接暴露
descriptor。

`object_type_id` 是 `type_id` 在对象系统里的别名。它由 C++ 对象类型的运行期 key 派生，不由对象类型名派生：

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

### objects::object_ptr

`objects::object_ptr<T>` 是非拥有对象指针，只保存对象指针，不延长对象生命周期：

```cpp
caors::objects::object_ptr<actor> ptr{&instance};
caors::object_id id = ptr.id();
```

它适合表达运行期对象关系。序列化时需要通过 `serialization::reference_resolver` 把对象指针转换为
`object_id`，反序列化时再把 `object_id` 解析回对象指针。`object_id` 本身仍是运行期标识，跨存档稳定
引用应由上层资源或对象数据库提供稳定 id 映射。

运行期值可以通过对象模块入口查询对象指针能力：

```cpp
caors::any_ref value = caors::any_ref::from(ptr);
auto object_ptr = caors::objects::object_ptr_view_of(value);
if (object_ptr) {
  caors::object* object = object_ptr.get();
}
```

### objects::object_handle 和 objects::typed_object_handle

`objects::object_handle` 是只保存 `object_id` 和 `object_type_id` 的身份描述，不拥有对象，也不提供解引用：

```cpp
caors::objects::object_handle object_handle{instance.id(), instance.type_id()};
```

`objects::typed_object_handle<T>` 是类型化身份句柄，内部同样只保存 `objects::object_handle`。它不会延长对象生命周期，
也不会隐式查询全局对象表；需要解析时应显式传给 `objects::manager` 或上层对象数据库。当前阶段如果需要把对象关系序列化为运行期对象 id，可以使用 `objects::object_ptr<T>` 和
`serialization::reference_resolver`，也可以从 `objects::manager` 创建 resolver。

### 对象引用计数

对象引用计数是可选所有权策略，而不是替换当前所有对象关系：

- `objects::object_ptr<T>` 继续表示非拥有裸引用，不增加引用计数。
- `objects::object_handle` / `objects::typed_object_handle<T>` 继续表示可解析身份，不隐式访问全局对象表，也不延长生命周期。
- `objects::manager` 继续作为明确拥有者，负责按创建时记录的 `memory_resource` 销毁对象。
- 序列化对象引用继续写出 `object_id` 或 `object_guid`，不序列化运行期引用计数。

第一阶段提供侵入式引用计数基础设施。对象类型显式继承 `objects::ref_counted`，并通过
`objects::make_ref<T>()` 创建由 `objects::strong_ref<T>` 拥有的对象。强引用在构造、拷贝、移动、析构和
`reset()` 时维护计数；最后一个强引用释放时，会按创建时传入的 `memory_resource` 销毁对象。
弱引用使用 `objects::weak_ref<T>` 表达，它不延长对象生命周期；对象仍存活时可以通过 `lock()`
尝试提升为强引用，对象销毁后 `lock()` 返回空强引用。

```cpp
class actor : public caors::objects::object_base<actor>,
              public caors::objects::ref_counted {
public:
  static constexpr std::string_view object_type_name = "game.actor";
};

caors::tracking_resource tracking(&caors::get_system_memory_resource());
caors::objects::strong_ref<actor> owner = caors::objects::make_ref<actor>(&tracking);
caors::objects::strong_ref<actor> copy = owner;
caors::objects::weak_ref<actor> weak = owner;

owner.reset(); // copy 仍然持有对象。

if (caors::objects::strong_ref<actor> locked = weak.lock()) {
  // 对象仍然存活。
}

copy.reset(); // 最后一个强引用释放，对象通过 tracking 销毁。
weak.lock();  // 返回空强引用。
```

`objects::ref_counted` 内部关联一个独立控制块。对象销毁后，只要仍存在 `objects::weak_ref<T>`，
控制块会继续保留，用于安全判断对象是否失效；最后一个弱引用释放后，控制块也会通过创建时使用的
`memory_resource` 释放。控制块中的强/弱计数使用原子变量，保证引用增减和 `weak_ref<T>::lock()`
可跨线程执行；这只保证生命周期控制，不表示对象内部状态可以并发读写。

引用计数和 manager 托管对象不应隐式混用。`objects::strong_ref<T>` 只管理由 `objects::make_ref<T>()`
创建的对象，不接管 `objects::manager` 已拥有的对象。若某个对象由 `objects::manager` 拥有，manager
仍是销毁入口；不能让普通 `objects::object_ptr<T>`、object_handle 或强引用包装偷偷接管对象所有权。若后续需要
manager 感知强引用，应显式增加 `destroy_later()`、延迟回收或“强引用归零后由 manager 销毁”的策略，
并补充失败路径测试。

`objects::weak_ref<T>` 和 `objects::typed_object_handle<T>` 解决的问题不同：

- `objects::weak_ref<T>` 面向引用计数对象，可以通过控制块安全判断对象是否仍由强引用持有。
- `objects::typed_object_handle<T>` 面向 manager / world 托管对象，只保存 `object_id` 和类型，必须显式通过
  `objects::manager` 或上层对象数据库解析。

因此场景树、world 或 manager 拥有的对象关系仍优先使用 `objects::object_ptr<T>` / `objects::typed_object_handle<T>`；
临时共享对象、资源对象或不挂在 manager 下的对象可以使用 `objects::strong_ref<T>` / `objects::weak_ref<T>`。

### objects::manager

`objects::manager` 是对象实例生命周期管理器。它拥有通过自身创建的对象，记录对象 id、注册类型和创建时使用的
`memory_resource`，并在 `destroy()` 或析构时释放对象：

```cpp
caors::objects::manager manager;

player* instance = manager.create<player>();
caors::objects::object_handle object_handle = manager.handle_of(instance->id());

player* resolved = manager.find<player>(object_handle);
manager.destroy(object_handle.id());
```

如果希望 manager 在创建和销毁对象时同步维护持久 GUID 映射，可以传入 `object_index`：

```cpp
caors::objects::object_index index;
caors::objects::manager manager(index);

player* instance = manager.create_with_guid<player>(persistent_guid);
player* resolved = static_cast<player*>(manager.find(persistent_guid));

manager.destroy(instance);
```

也可以创建已注册但非默认构造的对象：

```cpp
enemy* instance = manager.create<enemy>(spawn_level, spawn_position);
```

`manager.create(object_type_id)` 只支持已注册且默认构造的对象类型；`manager.create<T>(args...)`
要求 `T` 已注册，但允许构造参数。对象销毁时会使用 manager 记录的注册类型和内存资源，因此不依赖对象虚函数返回的类型 id。

类型化身份句柄不会自动解引用，必须显式通过 manager 查询：

```cpp
caors::objects::typed_object_handle<player> typed = manager.handle_of<player>(instance->id());

if (player* current = manager.find(typed)) {
  // 使用 current。
}
```

`manager` 当前不提供内部同步。创建、销毁、查找和遍历存活对象数量如果跨线程并发使用，调用方需要自行加锁。
`destroy()` 后，已有裸指针、`objects::object_ptr<T>` 和类型化身份句柄都不会再指向存活对象；类型化身份句柄可以再次通过
`manager.find()` 检测是否仍可解析。

当 manager 关联了 `object_index` 时，`destroy()` 和 `clear()` 会自动解除对象 id 对应的持久 GUID
绑定。未通过 manager 持有的对象不能通过 `manager.bind_guid()` 绑定。

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

`object_type_guid` 是可选静态成员。运行期临时对象可以不声明，`meta::type::guid()` 会保持 nil；可序列化、可反射或跨插件暴露的对象类型建议显式声明稳定 GUID。

`static_type_id()` 只查询已经注册的对象类型；对象类型尚未通过 `objects::class_<T>(name).commit()`
注册时返回无效 id。类型名、GUID 和父类型关系应在启动注册阶段一次性写入全局元数据注册表。

这样通过基类引用也能查询真实类型：

```cpp
caors::object& object = player_instance;
object.type_id();   // player::static_type_id()
object.type_name(); // "game.player"
```

### objects::registry

`objects::registry` 是基于全局 `meta::registry` 的对象注册表 facade 和默认构造工厂。它不表示彼此隔离的局部注册表实例。普通代码优先通过 `objects::class_<T>()`、`objects::find()`、`objects::id()` 和 `objects::is_a()` 访问对象元数据；需要默认构造工厂时再使用 `objects::get_registry()`。

对外查询返回 `objects::type` 只读视图。它组合了基础 `meta::type` 和内部 `objects::detail::type_data`，因此调用方可以通过 `id()`、`name()`、`guid()`、`size()`、`alignment()`、`parent_id()` 和 `constructible()` 查询对象类型信息，但不能直接修改注册表内部数据。

```cpp
if (caors::objects::class_<actor>(actor::object_type_name).commit()) {
  // 处理注册失败。
}

caors::error result =
    caors::objects::class_<player>(player::object_type_name).commit();
```

注册后可以查询类型：

```cpp
auto type = caors::objects::find<player>();
```

也可以检查继承关系：

```cpp
caors::objects::is_a(player::static_type_id(), actor::static_type_id());
```

注册失败会返回 `caors::error`，`domain()` 为 `error_domain::object`，`value()` 为明确的通用错误码：

- `errc::invalid_type`：类型描述无效。
- `errc::duplicate_id`：同一个运行期对象类型 id 已注册。
- `errc::duplicate_name`：同一个名称已注册。
- `errc::duplicate_guid`：同一个非 nil 稳定 GUID 已注册。
- `errc::missing_parent`：父类型未注册。

## 创建和销毁

注册表支持按 `object_type_id` 创建默认构造对象：

```cpp
caors::objects::registry& registry = caors::objects::get_registry();

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

`registry.destroy()` 需要传回与创建时一致的内存资源。对象本身不保存分配来源；如果希望由库记录分配来源，
应优先使用 `objects::manager`。

对于需要构造参数的类型，可以直接使用模板入口：

```cpp
auto* instance = caors::objects::create<player>(&resource, constructor_arg);
caors::objects::destroy(instance, &resource);
```

非默认构造类型仍可注册类型描述，但不能通过注册表默认创建。

生命周期管理器适合应用层持有运行期对象：

```cpp
caors::tracking_resource tracking(&caors::get_system_memory_resource());
caors::objects::manager manager(&tracking);

player* instance = manager.create<player>();
caors::objects::typed_object_handle<player> typed = manager.handle_of<player>(instance->id());

manager.destroy(instance);
```

## 当前已实现能力

- `object_id`：运行期对象实例标识。
- `objects::generate_object_id()`：全进程运行期对象 id 生成入口，正常返回非 0 且不复用的
  `object_id`。
- `object_guid`：对象系统对 `guid` 的语义别名，用于持久对象身份。
- `objects::object_index`：持久对象 GUID 和运行期对象 id 的双向索引。
- `object_type_id`：运行期对象类型标识。
- `object`：基础对象身份和类型查询入口。
- `object_base<Derived, Base>`：CRTP 类型查询辅助。
- `meta::type` / `meta::registry`：对象系统和反射系统共享的全局基础类型视图和元数据注册表。
- `objects::detail::type_data`：内部对象元数据组件，保存默认构造、创建和销毁函数。
- `objects::type`：对象类型只读视图，组合 `meta::type` 和内部 `objects::detail::type_data`。
- `caors::error`：明确对象类型注册失败原因和错误来源域。
- `objects::registry`：类型注册、查询、继承关系判断、默认构造工厂。
- `objects::manager`：对象实例生命周期管理器，持有对象、按 `object_id` 查找和销毁对象，可选同步
  `object_index`，并解析 `object_handle` / `typed_object_handle<T>`。
- `objects::ref_counted`：可选侵入式引用计数基类。
- `objects::strong_ref<T>`：引用计数对象的强引用，只管理 `objects::make_ref<T>()` 创建的对象。
- `objects::weak_ref<T>`：引用计数对象的弱引用，可通过 `lock()` 尝试提升为强引用。
- `objects::make_ref<T>()`：接入 `memory_resource` 创建引用计数对象并返回强引用。
- `objects::class_<T>()`：对象类型链式注册入口。
- `objects::id()` / `objects::find()` / `objects::is_a()`：对象类型查询入口；`objects::find()` 未命中时返回无效
  `objects::type`。
- `objects::get_registry()`：进程内默认对象注册表。
- `objects::create<T>()` / `objects::destroy<T>()`：接入 `memory_resource` 的直接对象创建和销毁入口。
- `serialization::make_reference_resolver(objects::manager&)`：为对象引用序列化创建基于运行期
  `object_id` 的 manager resolver。
- `serialization::make_persistent_reference_resolver(objects::manager&)`：为对象引用序列化创建基于
  `object_guid` 的 manager resolver，需要 manager 关联 `object_index`。

## 后续路线图

- [x] 对象实例 id。
- [x] 全局运行期对象 id 生成器。
- [x] 增加持久对象 GUID 语义别名和 `object_index` 双向映射。
- [x] 对象类型 id、名称、大小、对齐和父类型。
- [x] 复用全局 `meta::registry`。
- [x] 将 `objects::detail::type_data` 作为类型元数据组件挂载，并通过 `objects::type` 视图对外查询。
- [x] 注册错误状态和不一致类型描述检测。
- [x] 默认构造工厂。
- [x] 接入 `memory_resource`。
- [x] 明确 `object_handle` / `typed_object_handle<T>` 设计。
- [x] 增加对象生命周期管理器。
- [x] manager 可选联动 `object_index`，创建、销毁和清理时维护持久对象映射。
- [x] 设计对象引用计数边界，明确它与 `typed_object_handle<T>`、`objects::object_ptr<T>`、manager 所有权和序列化 resolver 的关系。
- [x] 实现可选侵入式引用计数基础类型和强引用包装。
- [x] 实现引用计数弱引用控制信息和 `objects::weak_ref<T>`。
- [ ] 设计延迟销毁和 manager 感知引用计数的策略。
- [ ] 与反射系统打通字段、方法和属性元信息。
- [ ] 与序列化系统打通类型名、版本和构造流程。
- [x] 设计持久对象 GUID / 业务对象 id，并与运行期 `object_id` 建立加载期映射。
- [ ] 评估是否需要线程安全对象容器或对象数据库。

## 相关入口

- 可运行示例位于 `examples/object/`，示例组织规则见 [examples/README.md](../examples/README.md)。
- 行为测试位于 `tests/object/`，测试组织规则见 [tests/README.md](../tests/README.md)。
