<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Runtime Facade

`runtime` 主题提供面向用户的一站式运行时类型 facade，是普通用户优先使用的注册和查询入口。
它不引入新的类型注册表，
而是组合现有 `meta`、`reflection` 和 `value` 能力：

- `runtime::type` 内部持有 `meta::type`，按需查询反射组件。
- `runtime::class_<T>("name")` 先注册 meta 类/普通值类型，再把字段和方法挂到 reflection 组件上。
- `runtime::enum_<T>("name")` 注册枚举 meta 类型，适合只需要稳定类型身份、GUID 和枚举值转换能力的场景。
- `runtime::instance` 组合 `value::any_ref`、`runtime::type`、`reflection::field` 和 `reflection::method`，
  提供面向实例的字段读写和方法调用入口。
- `runtime::object` 是对象系统适配视图，用于处理 `objects::ptr<T>` 这类对象指针字段。
- `runtime::type::create_value()` 仍使用 `value::any` 的低层值构造能力，不等价于对象系统 factory。

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

已注册反射字段和方法后，可以通过 `runtime::instance` 访问具体实例：

```cpp
actor value{};
caors::runtime::instance instance = caors::runtime::view(value);

caors::any health = instance.get("health");
instance.set("health", 120);
caors::any result = instance.call("add_health", 5);
```

对象指针字段属于对象系统语义，不直接放在 `reflection::field` 上。需要按对象关系读写时，使用
`runtime::object` 适配视图：

```cpp
auto object = caors::runtime::object_view(actor_object);
object.set_ptr("target", &target_object);
caors::object* target = object.get_ptr("target");
```

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
- 具体模块仍保留高级入口，例如 `meta::class_<T>()`、`meta::enum_<T>()`、
  `reflection::class_<T>()` 和 `serialization::class_<T>()`，用于更细粒度地控制注册顺序和模块依赖。

因此依赖方向仍保持为上层 facade 依赖底层模块，`meta` 不反向依赖 `reflection` 或 `value`。
