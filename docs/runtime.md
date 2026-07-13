<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Runtime Facade

`runtime` 主题提供面向用户的一站式运行时类型 facade，是普通用户优先使用的注册和查询入口。
它不引入新的类型注册表，
而是组合现有 `meta`、`reflection` 和 `value` 能力：

- `runtime::type` 内部持有 `meta::type`，按需查询反射组件。
- `runtime::class_<T>("name")` 先注册 meta 类/普通值类型，再把字段和方法挂到 reflection 组件上。
- `runtime::enum_<T>("name")` 注册枚举 meta 类型，适合只需要稳定类型身份、GUID 和枚举值转换能力的场景。
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
- 具体模块仍保留高级入口，例如 `meta::class_<T>()`、`meta::enum_<T>()`、
  `reflection::class_<T>()` 和 `serialization::class_<T>()`，用于更细粒度地控制注册顺序和模块依赖。

因此依赖方向仍保持为上层 facade 依赖底层模块，`meta` 不反向依赖 `reflection` 或 `value`。
