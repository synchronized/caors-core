<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Object

本文档说明 CAORS 对象系统的当前设计边界、已实现能力和后续扩展方向。当前对象系统处于基础阶段，目标是为后续反射、序列化、资源系统和编辑器工具提供稳定入口。

## 设计目标

CAORS 对象系统第一阶段只解决几件事：

- 对象实例有运行期唯一标识。
- 对象类型复用 `meta::type`，包含类型标识、名称、稳定 GUID、大小、对齐和父类型。
- 对象类型通过 `registration::type_<T>()` 写入 `meta::registry`。
- 对象模块提供按类型创建和销毁默认构造对象的语义入口。
- 对象创建和销毁可以接入 `memory_resource`。
- 对象生命周期管理器可以持有实例、按 `object_id` 查找对象，并解析弱句柄和序列化引用。

当前不会实现：

- GC。
- manager 感知的引用计数回收策略。
- 脚本绑定。
- 信号事件系统。
- 对象模块自行维护一套独立属性 / 方法反射。
- 对象模块自行实现格式后端或对象图存档策略。

对象指针、句柄、引用计数对象和多态对象的序列化适配位于 `caors/object/serialization.h`，具体读写、
resolver、构造策略和格式后端仍由 `serialization` 模块负责。

## 核心概念

### 命名空间

最小对象身份定义位于 `caors::meta` 命名空间，例如 `meta::object`、`meta::object_id` 和
`meta::object_base<T>`。`objects` 模块在此基础上只提供对象生命周期、句柄、引用和对象工厂等组合能力，例如
`objects::handle`、`objects::typed_handle<T>`、`objects::ptr<T>`、`objects::ref_counted`、
`objects::ref<T>`、`objects::weak_ref<T>`、`objects::object_index` 和 `objects::manager`。

属性、方法、metadata、schema 和动态调用能力都归属 `meta`。对象模块不再维护独立反射数据，也不把
`meta::detail` 的对象 traits 作为对象模块公共扩展点转发；对象注册器内部直接复用 `meta` 的对象类型信息。

为了保持常用 API 简洁，根命名空间保留带对象语义的别名：`caors::object`、`caors::object_id`、
`caors::object_type_id`、`caors::object_base<T>`、`caors::object_ptr<T>`、`caors::object_handle`、
`caors::typed_object_handle<T>`、`caors::object_ref<T>` 和 `caors::object_weak_ref<T>`。对象 manager、
object_index、序列化 resolver 和工厂 helper 等低频组合件统一使用 `caors::objects::*`。

`objects::ptr<T>`、`objects::handle` 和 `objects::typed_handle<T>` 的运行期读写函数表作为
`meta::capabilities` 模块静态能力声明，并接入 `meta::type` 静态能力查询链路。
拿得到 `meta::type` 时可通过 `objects::ptr_capability` 或
`objects::handle_capability` 查询类型级能力；拿到 `any_ref` 时，对象模块会从
`any_ref` 携带的 `meta::type` 查询并绑定为 `ptr_view` 或 `handle_view`。

### object_id

`object_id` 是运行期对象实例标识。`0` 保留为无效值。普通对象构造时会通过
`meta::generate_object_id()` 从 CAORS 核心库中的全局原子生成器取得新 id；`objects::manager`
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

对象系统当前只保证 `meta::generate_object_id()` 的并发调用安全。它只负责生成当前进程内唯一的
运行期 id，不访问对象实例、注册表或 manager。

其他对象系统组件默认不提供内部同步：

- `objects::manager`：创建、销毁、查找、遍历和 `object_index` 联动都需要调用方保证外部同步。
- `objects::object_index`：绑定、解绑和查询需要和对应对象生命周期保持一致，跨线程访问时由调用方加锁。
- `objects::ptr<T>`、`objects::handle` 和 `objects::typed_handle<T>`：只是轻量引用或身份描述，不延长对象生命周期，也不隐式保证对象仍然存活。
- 对象类型注册应在启动注册阶段完成；运行期并发注册和查询的策略由上层调度保证。

因此对象系统不会维护全局对象实例表，也不会在 `objects::ptr<T>` 或 `objects::handle` 中自动解引用全局状态。需要跨线程共享对象时，
建议在上层 manager、场景数据库或资源系统外包一层同步和生命周期策略。

### meta 类型信息

meta 类型信息由 `meta::registry` 内部保存，并通过 `meta::type` 对外查询。普通对象 API 默认使用全局
registry；序列化等 context-first 入口可以通过 `serialization::context::type_context` 指定本次读写使用的
`meta::context`。meta 类型信息包含：

- `id`
- `name`
- 可选 `type_guid`
- `size`
- `alignment`
- `parent_id`

其中名称、GUID 和父类型属于 meta 类型记录；大小、对齐和默认值操作函数通过同一 `type_id`
对应的 `meta::type` 查询。对象模块只通过 `meta::type` 查询这些信息，不直接暴露底层实现对象。

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

### objects::object_view

`objects::object_view` 是对象系统的高层反射访问门面。它不拥有对象，只保存外部实例引用，并在需要时
复用 `meta::instance` 读取属性、写入属性和调用方法。普通对象访问优先使用 `meta::value`：

```cpp
auto object = caors::objects::view(actor);

object.set("health", caors::meta::value{100});
caors::meta::value health = object.get("health");

caors::meta::invoke_value_result result = object.call("add_health", 5);
```

对象指针属性也走同一条高层路径：`objects::ptr<T>` 属性读出非空值时返回
`meta::value::object`，写入时可以传入 `meta::value::from_object(ptr)`、派生自
`caors::object` 的裸指针，或 `nullptr` 清空指针：

```cpp
object.set("target", caors::meta::value::from_object(&target));
caors::object* target_object = object.get("target").as_object();

object.set("target", nullptr);
```

`object_view::instance()` 可以取回底层 `meta::instance`，用于缓存 `meta::property` /
`meta::method_set` 后走热点路径。需要精确 C++ 类型、复杂参数、引用返回或低层桥接时，仍可使用
`get_ref()`、`get_any()`、`set_any()` 和 `call_any()`；这些属于高级入口，不是普通对象访问的首选。

### objects::ptr

`objects::ptr<T>` 是非拥有对象指针，只保存对象指针，不延长对象生命周期：

```cpp
caors::objects::ptr<actor> ptr{&instance};
caors::object_id id = ptr.id();
```

它适合表达运行期对象关系。序列化时需要通过 `objects::object_ref_resolver_ops` 把对象指针转换为
`object_id` 或 `object_guid`，反序列化时再解析回对象指针。`object_id` 本身仍是运行期标识，跨存档稳定
引用应使用 `object_guid`。

运行期值可以通过对象模块入口查询对象指针能力：

```cpp
caors::any_ref value = caors::any_ref::from(ptr);
auto ptr = value.get_capability<caors::objects::ptr_capability>();
if (ptr) {
  caors::object* object = ptr.get();
}
```

### 高级用法：对象 view 工厂

`objects::make_ptr_view(any_ref)` 和 `objects::make_handle_view(any_ref)` 是显式 view 工厂，
主要服务 capability 绑定、meta/对象桥接和高级适配代码。普通用户应优先使用
`any_ref::get_capability<objects::ptr_capability>()` /
`any_ref::get_capability<objects::handle_capability>()`。直接查询 `ptr_view` / `handle_view`
的旧方式不再保留。

如果只拿到类型视图，可以先查询类型级能力，再绑定到实例：

```cpp
auto capability =
    caors::meta::resolve<caors::objects::ptr<actor>>()
        .get_capability<caors::objects::ptr_capability>();
auto view = capability.bind(caors::any_ref::from(ptr));
```

如果只需要直接访问对象指针属性，也可以使用 `objects::object_view` 的显式指针 helper：

```cpp
auto object = caors::objects::view(owner);
object.set_ptr("target", &target);
caors::object* target = object.get_ptr("target");
```

`objects::view(owner)` 会显式绑定当前派生对象指针，因此 `object.ptr()` 可以返回 `&owner`。
如果只通过 `objects::view(any_ref)` 包装已经类型擦除的实例视图，不会尝试从派生类型自动反推出
`objects::object*`；这避免在没有完整对象类型语义时做不安全转换。需要当前对象指针时，
优先使用 `objects::view(T&)`，或由上层对象 manager / 场景系统显式提供。

`meta::property` 只保留通用属性读写能力，不直接暴露对象专用 `get/set` 接口。

如果 `objects::ptr<T>` / `objects::handle` 作为反射属性或容器元素使用，属性和容器元素 `any_ref`
会携带声明类型的 `meta::type`，属性读写和序列化路径不需要手动注册动态 capability。

它表达的是“当前已经拿到了对象地址”。如果对象由 `objects::manager`、场景树或其他上层容器拥有，
调用方必须保证 `ptr<T>` 的使用时间不超过对象生命周期；对象销毁后已有 `ptr<T>` 不会自动置空。

### objects::handle 和 objects::typed_handle

`objects::handle` 是只保存 `object_id` 和 `object_type_id` 的身份描述，不拥有对象，也不提供解引用：

```cpp
caors::objects::handle handle{instance.id(), instance.type_id()};
```

`objects::typed_handle<T>` 是类型化身份句柄，内部同样只保存 `objects::handle`。它不会延长对象生命周期，
也不会隐式查询全局对象表；需要解析时应显式传给 `objects::manager` 或上层对象数据库。当前阶段如果需要把对象关系序列化为运行期对象 id，可以使用 `objects::ptr<T>` 和
`objects::object_ref_resolver_ops`，也可以从 `objects::manager` 创建 resolver。

两者共享同一个内部句柄实现：`objects::handle` 是非类型化身份，`objects::typed_handle<T>` 是带
C++ 编译期类型约束的身份视图。类型化句柄可以从非类型化句柄构造，也可以通过 `.handle()` 取回
非类型化句柄：

```cpp
caors::objects::handle handle = manager.handle_of(instance->id());
caors::objects::typed_handle<player> typed{handle};

if (player* current = manager.find(typed)) {
  // 使用 current。
}
```

因此三类对象引用语义的边界是：

- `objects::ptr<T>`：保存地址，适合短期直接访问，不拥有对象。
- `objects::handle` / `objects::typed_handle<T>`：保存身份，适合 manager、world、场景树或序列化关系，必须通过 manager、对象数据库或资源系统解析。
- `objects::ref<T>` / `objects::weak_ref<T>`：保存引用计数所有权或弱观察关系，只管理 `objects::make_ref<T>()` 创建的对象。

### 对象引用计数

对象引用计数是可选所有权策略，而不是替换当前所有对象关系：

- `objects::ptr<T>` 继续表示非拥有裸引用，不增加引用计数。
- `objects::handle` / `objects::typed_handle<T>` 继续表示可解析身份，不隐式访问全局对象表，也不延长生命周期。
- `objects::manager` 继续作为明确拥有者，负责按创建时记录的 `memory_resource` 销毁对象。
- `objects::ptr<T>`、`objects::handle` 和 `objects::typed_handle<T>` 这类对象身份引用继续写出
  `object_id` 或 `object_guid`，不延长对象生命周期。
- `objects::ref<T>` 作为拥有型 inline value 序列化内部对象值，空引用写 `null`，不序列化运行期引用计数。

第一阶段提供侵入式引用计数基础设施。对象类型显式继承 `objects::ref_counted`，并通过
`objects::make_ref<T>()` 创建由 `objects::ref<T>` 拥有的对象。强引用在构造、拷贝、移动、析构和
`reset()` 时维护计数；最后一个强引用释放时，会按创建时传入的 `memory_resource` 销毁对象。
弱引用使用 `objects::weak_ref<T>` 表达，它不延长对象生命周期；对象仍存活时可以通过 `lock()`
尝试提升为强引用，对象销毁后 `lock()` 返回空强引用。
`objects::weak_ref<T>` 不作为普通属性序列化；需要保存弱关系时应使用 `objects::handle`、资源 id
或上层对象数据库可解析的身份。

`objects::ref<Derived>` 可以隐式转换为 `objects::ref<Base>`，`objects::weak_ref<Derived>`
也可以转换为 `objects::weak_ref<Base>`。转换只在 `Derived*` 可安全转换为 `Base*` 时启用，引用计数
控制块仍保存创建时的真实销毁函数，因此最后一个基类强引用释放时仍会按真实派生类型销毁对象。
不提供向下转换或横向转换；需要这类操作时应先通过对象类型信息显式判断。
序列化读入 `objects::ref<Base>` 的多态 envelope 时，也会创建真实派生对象并让强引用接管对应控制块；
引用计数仍是运行期状态，不写入存档。

```cpp
class actor : public caors::objects::object_base<actor>,
              public caors::objects::ref_counted {
public:
  static constexpr std::string_view object_type_name = "game.actor";
};

caors::tracking_resource tracking(&caors::get_system_memory_resource());
caors::objects::ref<actor> owner = caors::objects::make_ref<actor>(&tracking);
caors::objects::ref<actor> copy = owner;
caors::objects::weak_ref<actor> weak = owner;

owner.reset(); // copy 仍然持有对象。

if (caors::objects::ref<actor> locked = weak.lock()) {
  // 对象仍然存活。
}

copy.reset(); // 最后一个强引用释放，对象通过 tracking 销毁。
weak.lock();  // 返回空强引用。
```

`objects::ref_counted` 内部关联一个独立控制块。对象销毁后，只要仍存在 `objects::weak_ref<T>`，
控制块会继续保留，用于安全判断对象是否失效；最后一个弱引用释放后，控制块也会通过创建时使用的
`memory_resource` 释放。控制块中的强/弱计数使用原子变量，保证引用增减和 `weak_ref<T>::lock()`
可跨线程执行；这只保证生命周期控制，不表示对象内部状态可以并发读写。

引用计数和 manager 托管对象不应隐式混用。`objects::ref<T>` 只管理由 `objects::make_ref<T>()`
创建的对象，不接管 `objects::manager` 已拥有的对象。若某个对象由 `objects::manager` 拥有，manager
仍是销毁入口；不能让普通 `objects::ptr<T>`、`objects::handle` 或强引用包装偷偷接管对象所有权。
如果在事件派发、对象遍历或上层场景更新中不能立即释放对象，应使用 manager 的延迟销毁队列，而不是让
`objects::ref<T>` 接管 manager 对象。

`objects::weak_ref<T>` 和 `objects::typed_handle<T>` 解决的问题不同：

- `objects::weak_ref<T>` 面向引用计数对象，可以通过控制块安全判断对象是否仍由强引用持有。
- `objects::typed_handle<T>` 面向 manager / world 托管对象，只保存 `object_id` 和类型，必须显式通过
  `objects::manager` 或上层对象数据库解析。

因此场景树、world 或 manager 拥有的对象关系仍优先使用 `objects::ptr<T>` / `objects::typed_handle<T>`；
临时共享对象、资源对象或不挂在 manager 下的对象可以使用 `objects::ref<T>` / `objects::weak_ref<T>`。

### objects::manager

`objects::manager` 是对象实例生命周期管理器。它拥有通过自身创建的对象，记录对象 id、注册类型和创建时使用的
`memory_resource`，并在 `destroy()`、`flush_destroy_queue()`、`clear()` 或析构时释放对象：

```cpp
caors::objects::manager manager;

player* instance = manager.create<player>();
caors::objects::handle handle = manager.handle_of(instance->id());

player* resolved = manager.find<player>(handle);
manager.destroy(handle.id());
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
caors::objects::typed_handle<player> typed = manager.handle_of<player>(instance->id());

if (player* current = manager.find(typed)) {
  // 使用 current。
}
```

`manager` 当前不提供内部同步。创建、销毁、查找和遍历存活对象数量如果跨线程并发使用，调用方需要自行加锁。
`destroy()` 后，已有裸指针、`objects::ptr<T>` 和类型化身份句柄都不会再指向存活对象；类型化身份句柄可以再次通过
`manager.find()` 检测是否仍可解析。

需要延迟释放对象时，可以调用 `destroy_later()`。它会立即把对象从 manager 查找表和 `object_index`
中移除，使 `handle` / `typed_handle<T>` 逻辑失效；对象实例本身会进入销毁队列，直到调用
`flush_destroy_queue()`、`clear()` 或 manager 析构时才真正析构和释放：

```cpp
caors::objects::handle handle = manager.handle_of(instance->id());

manager.destroy_later(instance);

manager.find(handle);           // 返回 nullptr，逻辑上已经失效。
manager.pending_destroy_count(); // 返回等待物理释放的对象数量。

manager.flush_destroy_queue();  // 真正析构并释放延迟队列中的对象。
```

延迟销毁只解决 manager 拥有对象的“逻辑失效”和“物理释放”分离问题，不提供引用计数，也不保证裸指针安全。
调用方如果在 `destroy_later()` 后临时保留原始指针，只能在明确知道 `flush_destroy_queue()` 尚未执行时使用；
更推荐用 `handle` 重新解析对象，并把无法解析视为对象已经死亡。

当 manager 关联了 `object_index` 时，`destroy()`、`destroy_later()` 和 `clear()` 会自动解除对象 id 对应的持久 GUID
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

`static_type_id()` 返回 C++ 类型 id；对象类型尚未通过 `registration::type_<T>().commit()`
注册时，`objects::find<T>()`、对象工厂、类型名、GUID 和父类型关系仍不可用。对象类型应在启动注册阶段
一次性写入同一个 `meta::context` 对应的元数据注册表。

这样通过基类引用也能查询真实类型：

```cpp
caors::object& object = player_instance;
object.type_id();   // player::static_type_id()
object.type_name(); // "game.player"
```

### 对象类型 API

对象模块不维护独立 registry。对象类型通过 `registration::type_<T>()` 或
`registration::type_<T>(context, name)` 注册到 `meta::registry`；只要 `T` 继承自
`objects::object`，`commit()` 会自动写入对象工厂能力。注册后通过 `objects::find()`、
`objects::id()`、`objects::is_a()`、`objects::create()` 和 `objects::destroy()` 使用对象语义入口。
无 context 重载使用 `meta::default_context()`；需要隔离注册表时显式传入同一个 `meta::context`。

对外查询直接返回 `meta::type` 只读视图。调用方可以通过 `id()`、`name()`、`guid()`、`size()`、
`alignment()` 和 `parent_id()` 查询对象类型信息；对象专有语义通过
`objects::contains(type)`、`objects::factory(type)` 和 `objects::constructible(type)` 查询。`meta::type`
仍然不能直接修改注册表内部数据。

```cpp
if (caors::registration::type_<actor>().commit()) {
  // 处理注册失败。
}

caors::error result = caors::registration::type_<player>().commit();
```

无参 `registration::type_<T>()` 会读取 `T::object_type_name`。需要非常规显式覆盖类型名时，可以使用
`registration::type_<T>("custom.name")`；常规对象类型不应重复传入 `T::object_type_name`。

注册后可以查询类型：

```cpp
auto type = caors::objects::find<player>();

caors::meta::registry local_registry;
caors::meta::context local_context{local_registry};
auto local_type = caors::objects::find<player>(local_context);
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

对象语义 API 优先支持按 `meta::type` 创建默认构造对象：

```cpp
caors::meta::type player_type = caors::objects::find<player>();
caors::object* instance = caors::objects::create(player_type);
caors::objects::destroy(player_type, instance);
```

也可以把 `object_type_id` 或对象类型名作为便捷入口传入，内部会先查询同一个 `meta::context`
中的 `meta::type`。

也可以直接使用 C++ 类型创建已注册对象：

```cpp
player* instance = caors::objects::create<player>();
caors::objects::destroy(instance);
```

也可以显式传入内存资源：

```cpp
caors::tracking_resource tracking(&caors::get_system_memory_resource());

player* instance = caors::objects::create<player>(&tracking);
caors::objects::destroy(instance, &tracking);
```

`objects::destroy()` 需要传回与创建时一致的内存资源。对象本身不保存分配来源；如果希望由库记录分配来源，
应优先使用 `objects::manager`。

对于需要构造参数的类型，可以直接使用模板入口：

```cpp
auto* instance = caors::objects::create<player>(&resource, constructor_arg);
caors::objects::destroy(instance, &resource);
```

非默认构造类型仍可注册类型描述，但不能通过 `objects::create(meta::type)`、`objects::create(object_type_id)`
或对象类型名默认创建。

生命周期管理器适合应用层持有运行期对象：

```cpp
caors::tracking_resource tracking(&caors::get_system_memory_resource());
caors::objects::manager manager(&tracking);

player* instance = manager.create<player>();
caors::objects::typed_handle<player> typed = manager.handle_of<player>(instance->id());

manager.destroy(instance);
```

## 当前已实现能力

- `object_id`：运行期对象实例标识。
- `meta::generate_object_id()`：全进程运行期对象 id 生成入口，正常返回非 0 且不复用的
  `object_id`。
- `object_guid`：对象系统对 `guid` 的语义别名，用于持久对象身份。
- `objects::object_index`：持久对象 GUID 和运行期对象 id 的双向索引。
- `object_type_id`：运行期对象类型标识。
- `object`：基础对象身份和类型查询入口。
- `object_base<Derived, Base>`：CRTP 类型查询辅助。
- `meta::type` / `meta::registry`：对象系统和 meta 属性/方法共享的全局类型视图和元数据注册表。
- `objects::object_factory_capability`：对象工厂公开 facade，保存默认构造、创建和销毁能力。
- `objects::object_factory_ops`：对象工厂 raw ops 函数表，由 registry 内部和 `meta::type` 静态能力使用。
- `objects::contains(meta::type)` / `objects::factory()` / `objects::constructible()`：基于 `meta::type`
  查询对象类型语义和对象工厂能力。
- `caors::error`：明确对象类型注册失败原因和错误来源域。
- `objects::create()` / `objects::destroy()`：基于 `meta::type` 和对象工厂能力的默认构造工厂入口。
- `objects::manager`：对象实例生命周期管理器，持有对象、按 `object_id` 查找和销毁对象，可选同步
  `object_index`，支持延迟销毁队列，并解析 `objects::handle` / `objects::typed_handle<T>`。
- `objects::ref_counted`：可选侵入式引用计数基类。
- `objects::ref<T>`：引用计数对象的强引用，只管理 `objects::make_ref<T>()` 创建的对象。
- `objects::weak_ref<T>`：引用计数对象的弱引用，可通过 `lock()` 尝试提升为强引用。
- `objects::make_ref<T>()`：接入 `memory_resource` 创建引用计数对象并返回强引用。
- `registration::type_<T>()`：对象类型链式注册入口。
- `objects::id()` / `objects::find()` / `objects::is_a()`：对象类型查询入口；`objects::find()` 未命中时返回无效
  `meta::type`。
- `objects::create<T>()` / `objects::destroy<T>()`：接入 `memory_resource` 的直接对象创建和销毁入口。
- `objects::make_object_ref_resolver(objects::manager&)`：为对象引用序列化创建基于运行期
  `object_id` 的 manager resolver，并挂到 `serialization::context::resolvers.external_refs.resolver`。
- `objects::make_persistent_object_ref_resolver(objects::manager&)`：为对象引用序列化创建基于
  `object_guid` 的 manager resolver，需要 manager 关联 `object_index`，同样通过
  `serialization::context::resolvers.external_refs.resolver` 接入。
- `objects::make_object_factory(objects::manager&)`：为多态对象反序列化创建通用类型工厂，并挂到
  `serialization::context::resolvers.factories.polymorphic_factory`。

## 后续路线图

- [x] 对象实例 id。
- [x] 全局运行期对象 id 生成器。
- [x] 增加持久对象 GUID 语义别名和 `object_index` 双向映射。
- [x] 对象类型 id、名称、大小、对齐和父类型。
- [x] 复用全局 `meta::registry`。
- [x] 将底层 `objects::object_factory_ops` 挂载到 meta capability 索引，并通过
  `meta::type`、`objects::object_factory_capability` 和对象语义 helper 对外查询。
- [x] 注册错误状态和不一致类型描述检测。
- [x] 默认构造工厂。
- [x] 接入 `memory_resource`。
- [x] 明确 `objects::handle` / `objects::typed_handle<T>` 设计。
- [x] 增加对象生命周期管理器。
- [x] manager 可选联动 `object_index`，创建、销毁和清理时维护持久对象映射。
- [x] 设计对象引用计数边界，明确它与 `objects::typed_handle<T>`、`objects::ptr<T>`、manager 所有权和序列化 resolver 的关系。
- [x] 实现可选侵入式引用计数基础类型和强引用包装。
- [x] 实现引用计数弱引用控制信息和 `objects::weak_ref<T>`。
- [x] 设计并实现 manager 延迟销毁策略，明确它和引用计数对象的边界。
- [x] 与 meta 属性/方法打通对象高层访问和属性元信息。
- [x] 与序列化系统打通对象指针、句柄、引用计数对象、多态 object 和对象工厂 resolver。
- [x] 设计持久对象 GUID / 业务对象 id，并与运行期 `object_id` 建立加载期映射。
- [ ] 设计完整场景 / 对象图存档策略，包括对象图版本、共享身份、循环引用和加载期修复流程。
- [ ] 评估是否需要线程安全对象容器或对象数据库。

## 相关入口

- 可运行示例位于 `examples/object/`，示例组织规则见 [examples/README.md](../examples/README.md)。
- 行为测试位于 `tests/object/`，测试组织规则见 [tests/README.md](../tests/README.md)。
- 对象序列化行为测试位于 `tests/serialization/object/`，多态对象读写测试位于
  `tests/serialization/polymorphic/`。
