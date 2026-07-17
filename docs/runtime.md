<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Runtime Facade

`runtime` 主题提供面向用户的一站式运行时类型 facade，是普通用户优先使用的注册和查询入口。
它不引入新的类型注册表，
而是组合现有 `meta` 属性/方法和 meta value 能力：

- `runtime::type` 内部持有 `meta::type`，按需查询属性和方法。
- `runtime::class_<T>("name")` 先注册 meta 类/普通值类型，再把属性和方法写入同一份 meta 类型数据。
- `runtime::enum_<T>("name")` 注册枚举 meta 类型，适合只需要稳定类型身份、GUID 和枚举值转换能力的场景。
- `runtime::instance` 组合 `meta::any_ref`、`runtime::type`、`meta::property`、`meta::method_set`
  和 `meta::method`，
  提供面向实例的属性读写和方法调用入口。
- `runtime::type::create()` 是运行期值创建的高层入口，优先使用 `meta::factory_capability`，再使用
  `meta::constructor_capability` 匹配运行期参数列表，最后才回退到 descriptor 默认构造。
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
    .property("health", &actor::health)
    .method("add_health", &actor::add_health)
    .commit();

caors::runtime::type type = caors::runtime::find<actor>();
caors::runtime::field field = type.find_property("health");
```

## 创建值

`runtime::type::create()` 是普通代码优先使用的运行期创建入口：

```cpp
caors::runtime::type type = caors::runtime::find<actor>();
caors::any value = type.create();
```

当前创建优先级是：

1. 如果类型注册了 `meta` factory，使用 factory 构造对象，并让返回的 `any` 记住匹配的内部销毁策略。
2. 如果传入运行期参数列表，使用已注册 constructor capability 精确匹配参数数量和参数类型。
3. 如果没有参数且没有可用 factory / constructor，回退到 descriptor 默认构造，也就是
   `any::default_construct()`。
4. 没有匹配构造策略时返回无效 `any`。

需要指定分配器时，把 `memory_resource` 传给 `create()`：

```cpp
caors::tracking_resource tracking{&caors::get_system_memory_resource()};
caors::any value = type.create(&tracking);
```

需要按运行期参数构造时，先注册 constructor，再把参数包装成 `any_ref` 列表传给 `create()`：

```cpp
struct actor {
  explicit actor(int health) : health(health) {}
  int health = 0;
};

caors::runtime::class_<actor>("game.actor")
    .constructor<int>()
    .commit();

int health = 100;
std::array<caors::any_ref, 1> args{caors::any_ref::from(health)};

caors::runtime::type type = caors::runtime::find<actor>();
caors::any value =
    type.create(std::span<const caors::any_ref>{args.data(), args.size()});
```

constructor 参数采用保守匹配策略：参数数量必须一致，参数类型必须和注册的 `Args...`
去 cv/ref 后精确匹配；非 const 左值引用参数要求传入可写 `any_ref`。当前版本只支持一个 constructor
能力；右值引用参数和 move-only by-value 参数暂不支持。构造函数重载选择、默认参数和隐式类型转换后续再扩展。

### 高级用法：值构造与所有权接管

`create_value()` 是高级默认构造入口，只表达“这个 C++ 类型能否默认构造为一个值”。它不会查询
`meta::factory_capability`，因此不适合作为多态反序列化、编辑器创建对象或资源配置加载的首选入口。
它适合测试、兜底构造、工具层显式绕过 factory 的场景。

更底层的 `meta::any::default_construct()`、`meta::owned_value` 和 `meta::any::take_ownership()`
属于 meta value 的高级构造接口：

- `default_construct(type, resource)` 只按 descriptor 默认构造值。
- `factory_capability::construct_owned()` / `constructor_capability::construct_owned()` 返回
  `meta::owned_value`，适合工具层先填充对象再转交所有权；最终可用 `into_any()` 转成 `any`。
- `meta::make_owned_value(type, value, destroy)` / `meta::make_owned_value(type, value, resource, destroy)`
  可以把外部高级构造结果封装进 RAII 句柄；调用方必须保证销毁函数和资源匹配。
- `take_ownership(owned_value&&)` 接管 `owned_value` 持有的对象，并保留匹配的资源销毁函数或普通销毁函数。
  普通 meta factory 调用优先使用 `factory_capability::construct()`。
- 普通业务、编辑器创建、多态反序列化和资源配置加载应优先通过 `runtime::type::create()` 统一调度。

注册自定义无参构造策略时，使用 `runtime::class_<T>().factory()`：

```cpp
void* construct_actor(caors::memory_resource* resource);
void destroy_actor(caors::memory_resource* resource, void* value) noexcept;

caors::runtime::class_<actor>("game.actor")
    .factory(&construct_actor, &destroy_actor)
    .property("health", &actor::health)
    .commit();
```

`factory()` 适合自定义无参创建、对象池、外部分配策略或多态反序列化；`constructor<Args...>()`
适合编辑器、脚本桥接和工具层按运行期参数创建值。两者同时存在时，无参 `create()` 优先使用
factory；带参数 `create(arguments)` 只匹配 constructor。

已注册反射属性和方法后，可以通过 `runtime::instance` 访问具体实例：

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
- `get()` 返回属性值拷贝，属性不存在或值不可拷贝时返回无效 `any`。
- `get_ref()` 返回属性引用；只读实例返回只读 `any_ref`。
- `set()` 只在实例可写、属性存在并且类型匹配时成功。
- `call()` 按反射方法调用；只读实例只能调用 `const` 成员函数，非 const 成员函数会返回无效 `any`。
- 无效实例、未注册反射类型、属性/方法未命中时，查询返回空视图、空值或 `false`。

`properties()` 和 `methods()` 当前返回属性/方法视图的 `core::vector` 快照，适合简单遍历。它不表达成员生命周期所有权；
属性和方法视图仍引用全局 append-only `meta::registry`。后续如果需要零分配遍历，可以再增加 range view。

对象指针属性属于对象系统语义，不直接放在 `meta::property` 上。需要按对象关系读写时，使用
`objects::object_view` / `objects::view(...)`，详见对象系统文档。

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
- `meta` 负责属性、方法和成员访问。
- `runtime` 只负责把这些能力组合成更直接的用户入口。
- object/resource 等领域专用便利能力放在对应模块或适配层，不反向塞进 `meta::property`
  这类底层通用视图。
- `runtime` 组合公开 facade，不直接依赖 `meta::detail`。需要构造未知类型值时走
  `runtime::type::create()`、`meta::factory_capability`、`meta::constructor_capability`
  或 `meta::any` / `meta::owned_value` 这类公开所有权接口。
- 具体模块仍保留高级入口，例如 `meta::class_<T>()`、`meta::enum_<T>()`、
  `meta::class_<T>()` 和 `serialization::class_<T>()`，用于更细粒度地控制注册顺序和模块依赖。

因此依赖方向仍保持为上层 facade 依赖底层模块。
