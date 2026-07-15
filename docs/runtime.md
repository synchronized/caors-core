<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Runtime Facade

`runtime` 主题提供面向用户的一站式运行时类型 facade，是普通用户优先使用的注册和查询入口。
它不引入新的类型注册表，
而是组合现有 `meta`、`reflection` 和 meta value 能力：

- `runtime::type` 内部持有 `meta::type`，按需查询反射组件。
- `runtime::class_<T>("name")` 先注册 meta 类/普通值类型，再把字段和方法挂到 reflection 组件上。
- `runtime::enum_<T>("name")` 注册枚举 meta 类型，适合只需要稳定类型身份、GUID 和枚举值转换能力的场景。
- `runtime::instance` 组合 `meta::any_ref`、`runtime::type`、`reflection::field` 和 `reflection::method`，
  提供面向实例的字段读写和方法调用入口。
- `runtime::object` 是对象系统适配视图，用于处理 `objects::ptr<T>` 这类对象指针字段。
- `runtime::type::create()` 是运行期值创建的高层入口，优先使用 `meta::factory_capability`，最后才回退到
  descriptor 默认构造。
- `runtime::type::create_value()` 是高级默认构造入口，只使用 `meta::any` 的 descriptor 默认构造能力，
  不查询 factory。

## 基本使用

```cpp
struct actor {
  int health = 100;

  int add_health(int delta) {
    health += delta;
    return health;
  }
};

caors::runtime::class_<actor>("game.actor")
    .field("health", &actor::health)
    .method("add_health", &actor::add_health)
    .commit();

caors::runtime::type type = caors::runtime::find<actor>();
caors::runtime::field field = type.find_field("health");
```

## 创建值

`runtime::type::create()` 是普通代码优先使用的运行期创建入口：

```cpp
caors::runtime::type type = caors::runtime::find<actor>();
caors::any value = type.create();
```

当前创建优先级是：

1. 如果类型注册了 `meta` factory，使用 factory 构造对象，并让返回的 `any` 记住匹配的销毁函数。
2. 如果没有 factory，回退到 descriptor 默认构造，也就是 `any::default_construct()`。
3. 两者都不可用时返回无效 `any`。

需要指定分配器时，把 `memory_resource` 传给 `create()`：

```cpp
caors::tracking_resource tracking{&caors::get_system_memory_resource()};
caors::any value = type.create(&tracking);
```

### 高级用法：值构造与所有权接管

`create_value()` 是高级默认构造入口，只表达“这个 C++ 类型能否默认构造为一个值”。它不会查询
`meta::factory_capability`，因此不适合作为多态反序列化、编辑器创建对象或资源配置加载的首选入口。
它适合测试、兜底构造、工具层显式绕过 factory 的场景。

更底层的 `meta::any::default_construct()` 和 `meta::any::take_ownership()` 属于 meta value 的高级构造接口：

- `default_construct(type, resource)` 只按 descriptor 默认构造值。
- `take_ownership(type, value, resource, destroy)` 接管外部工厂创建的对象，调用方必须保证对象类型、
  销毁函数和 `memory_resource` 完全匹配。
- 普通业务、编辑器创建、多态反序列化和资源配置加载应优先通过 `runtime::type::create()` 统一调度。

注册自定义构造策略时，使用 `runtime::class_<T>().factory()`：

```cpp
void* construct_actor(caors::memory_resource* resource);
void destroy_actor(caors::memory_resource* resource, void* value) noexcept;

caors::runtime::class_<actor>("game.actor")
    .factory(&construct_actor, &destroy_actor)
    .field("health", &actor::health)
    .commit();
```

独立的“反射构造函数/带参 constructor 注册”尚未实现。后续如果引入 constructor 组件，`create()`
会继续作为统一入口，在 factory capability 和 descriptor 默认构造之间加入对应策略。

已注册反射字段和方法后，可以通过 `runtime::instance` 访问具体实例：

```cpp
actor value{};
caors::runtime::instance instance = caors::runtime::view(value);

caors::any health = instance.get("health");
instance.set("health", 120);
caors::any result = instance.call("add_health", 5);
```

`runtime::instance` 是非拥有视图，只保存 `meta::any_ref`：

- `runtime::view(value)` 创建可写实例视图。
- `runtime::view_const(value)` 创建只读实例视图。
- `get()` 返回字段值拷贝，字段不存在或值不可拷贝时返回无效 `any`。
- `get_ref()` 返回字段引用；只读实例返回只读 `any_ref`。
- `set()` 只在实例可写、字段存在并且类型匹配时成功。
- `call()` 按反射方法调用；只读实例只能调用 `const` 成员函数，非 const 成员函数会返回无效 `any`。
- 无效实例、未注册反射类型、字段/方法未命中时，查询返回空视图、空值或 `false`。

`fields()` 和 `methods()` 当前返回字段/方法视图的 `core::vector` 快照，适合简单遍历。它不表达成员生命周期所有权；
字段和方法视图仍引用全局 append-only 反射注册表。后续如果需要零分配遍历，可以再增加 range view。

对象指针字段属于对象系统语义，不直接放在 `reflection::field` 上。需要按对象关系读写时，使用
`runtime::object` 适配视图：

```cpp
auto object = caors::runtime::object_view(actor_object);
object.set_ptr("target", &target_object);
caors::object* target = object.get_ptr("target");
```

`runtime::object` 也是非拥有视图：

- `runtime::object_view(T&)` 要求 `T` 派生自 `objects::object`，会显式保存当前对象指针，`ptr()` 返回该指针。
- `runtime::object_view(instance)` 只包装已有实例视图，不会从类型擦除的派生对象自动推导
  `objects::object*`；没有显式对象指针时 `ptr()` 可能返回 `nullptr`。
- `get_ptr(name)` / `set_ptr(name, ptr)` 只处理字段类型具备 `objects::ptr<T>` 语义的字段；
  普通裸指针、普通字段或不存在字段会返回 `nullptr` / `false`。
- `runtime::object` 不管理对象生命周期，也不解析 `objects::handle`、`objects::typed_handle<T>`、
  `objects::ref<T>` 或 `objects::weak_ref<T>`。这些所有权或身份策略应继续由对象系统、manager、
  场景树或上层 runtime 适配层处理。

枚举类型可以通过 `runtime::enum_<T>()` 注册：

```cpp
enum class actor_state {
  idle,
  running,
};

caors::runtime::enum_<actor_state>("game.actor_state").commit();

caors::runtime::type state_type = caors::runtime::find<actor_state>();
```

## 边界

`runtime` 是 API facade，不是底层模块合并：

- `meta` 继续负责类型身份、descriptor、registry 和 capability。
- `value` 继续负责 `any`、`any_ref` 和运行期 view。
- `reflection` 继续负责字段、方法和成员访问。
- `runtime` 只负责把这些能力组合成更直接的用户入口。
- object/resource 等领域专用便利能力放在 `runtime` 或对应适配层，不反向塞进 `reflection::field`
  这类底层通用视图。
- `runtime::object` 当前只收口对象指针字段访问和当前对象指针查询，不替代对象 manager、handle、
  引用计数或资源引用 resolver。
- 具体模块仍保留高级入口，例如 `meta::class_<T>()`、`meta::enum_<T>()`、
  `reflection::class_<T>()` 和 `serialization::class_<T>()`，用于更细粒度地控制注册顺序和模块依赖。

因此依赖方向仍保持为上层 facade 依赖底层模块，`meta` 不反向依赖 `reflection` 或 `value`。
