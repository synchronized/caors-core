<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Runtime Facade

`runtime` 主题提供面向用户的一站式运行时类型 facade。它不引入新的类型注册表，
而是组合现有 `meta`、`reflection` 和 `value` 能力：

- `runtime::type` 内部持有 `meta::type`，按需查询反射组件。
- `runtime::class_<T>("name")` 先注册 meta 类型，再把字段和方法挂到 reflection 组件上。
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

## 边界

`runtime` 是 API facade，不是底层模块合并：

- `meta` 继续负责类型身份、descriptor、registry 和 capability。
- `value` 继续负责 `any`、`any_ref` 和运行期 view。
- `reflection` 继续负责字段、方法和成员访问。
- `runtime` 只负责把这些能力组合成更直接的用户入口。

因此依赖方向仍保持为上层 facade 依赖底层模块，`meta` 不反向依赖 `reflection` 或 `value`。
