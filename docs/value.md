<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Value

value 模块负责运行期值访问和数据中间表示，主要包含四类能力：

- `any`：拥有一份 C++ 对象拷贝，保留运行期类型描述。
- `any_ref`：非拥有 C++ 对象引用，用于字段、容器元素和外部对象的零拷贝访问。
- 各类 view：把指针、wrapper、sequence、set、associative、pair、tuple 和 variant-like 类型按统一接口访问。
- `variant`：脱离精确 C++ 类型的数据 DOM，主要用于序列化、配置和跨格式转换。

## 头文件边界

value 模块提供细粒度头和完整聚合头。细粒度头不再依赖 `any.h` 间接带出其它能力，调用方需要按实际使用显式包含。

| 头文件 | 提供内容 | 适用场景 |
| --- | --- | --- |
| `caors/value/any.h` | `caors::value::any`、`caors::any` | 只需要拥有式运行期 C++ 值。 |
| `caors/value/any_ref.h` | `caors::value::any_ref`、`caors::any_ref` | 需要引用外部 C++ 对象或反射字段。 |
| `caors/value/view_factory.h` | 所有具体 view 类型 | 需要把 `any` / `any_ref` 创建成容器、wrapper、tuple 等运行期视图。 |
| `caors/value/associative_entry_ref.h` | `associative_entry_ref` | 需要按值使用关联容器迭代条目。 |
| `caors/value/variant.h` | `caors::value::variant`、`caors::variant` | 需要序列化 DOM 或数据中间表示。 |
| `caors/value.h` | value 模块完整公共入口 | 应用层或示例代码需要完整 value 能力。 |

`any.h` 和 `any_ref.h` 中会声明 `create_*_view()`，但不会提供具体 view 的完整定义。只保存返回值、调用 view 成员函数或按值声明 view 类型时，应额外包含 `caors/value/view_factory.h` 或对应具体 view 头。

## 基本用法

只使用拥有式值：

```cpp
#include <caors/value/any.h>

caors::any value = caors::any::from(42);
const int* stored = value.try_get<int>();
```

引用外部对象：

```cpp
#include <caors/value/any_ref.h>

int number = 1;
caors::any_ref ref = caors::any_ref::from(number);
ref.try_set(2);
```

创建运行期视图：

```cpp
#include <caors/core/containers.h>
#include <caors/value/any_ref.h>
#include <caors/value/view_factory.h>

caors::vector<int> numbers{1, 2, 3};
caors::any_ref ref = caors::any_ref::from(numbers);

caors::value::sequence_view view = ref.create_sequence_view();
for (caors::any_ref element : view) {
  element.try_set(0);
}
```

应用层如果不关心编译边界，可以直接使用完整入口：

```cpp
#include <caors/value.h>

caors::variant data = caors::variant::object({});
caors::any value = caors::any::from(7);
```

## any

`any` 拥有对象副本，适合把字段值从原对象中取出后独立保存。它依赖 meta descriptor 执行默认构造、复制、销毁和赋值。
`any::default_construct(type_id, memory_resource*)` 可以用指定 `memory_resource` 默认构造对象，返回的
`any` 会记住匹配的资源销毁函数；没有传入资源或类型缺少资源构造函数表时，回退到普通 `new` / `delete`
路径。

资源边界：

- 传入 `memory_resource*` 时，`any` 会优先使用 descriptor 中的资源构造/销毁函数。
- 不传资源时，`any` 使用普通 `new` / `delete` fallback，不读取或保存 CAORS 默认资源。
- 复制已有值生成 `any` 时仍按普通 clone 路径工作；需要指定资源创建空对象再读入数据时，应使用
  `any::default_construct(type_id, resource)`。

当前限制：

- 不可拷贝类型无法被复制成有效 `any`。
- `any` 不保留对象身份、共享关系或对象图引用。
- 未注册 C++ 类型仍可通过 descriptor 使用基础值能力，但没有稳定名称、GUID、反射字段或多态构造信息。

## any_ref

`any_ref` 不拥有对象，只保存对象地址、运行期类型信息和可写性。调用方必须保证被引用对象在 `any_ref` 使用期间仍然存活。

常见来源：

- `caors::any_ref::from(value)`：引用普通 C++ 对象。
- `reflection::field::get_ref(object)`：引用反射字段。
- view 的 `element_ref()`、`value_ref()`、`first_ref()` 等：引用容器元素或结构内部值。

`any_ref` 可以写回同类型值；只读引用、类型不匹配或底层类型不支持赋值时返回失败。

## 运行期视图

运行期视图是轻量非拥有对象，不延长底层对象、容器元素或 wrapper 内部值的生命周期。

当前视图包括：

- `indirect_view`：访问裸指针、智能指针、optional、reference wrapper 等间接值。
- `sequence_view`：访问顺序容器。
- `set_view`：访问集合容器。
- `associative_view`：访问 key/value 关联容器。
- `pair_view`：访问 pair-like 类型。
- `tuple_view`：访问 tuple-like 类型。
- `variant_view`：访问 variant-like 类型。

这些 view 通过 `meta::detail::type_descriptor` 保存的类型能力工作，不要求容器类型本身一定注册到全局 meta registry。需要名称、GUID、反射字段、序列化策略或多态构造的业务类型仍应显式注册。

集合和关联容器的 erased iterator 是 value/meta 内部临时对象，使用 CAORS 内部资源策略分配，并在 iterator
视图析构时自动释放。它不使用用户容器的 allocator，也不会延长底层容器元素生命周期。

## variant

`variant` 是数据 DOM，不保留精确 C++ 类型，只表达 null、bool、整数、浮点、字符串、数组、字典和对象等格式友好的数据形态。

它和 `any` 的边界：

- `any` 面向 C++ 类型和反射字段访问。
- `variant` 面向序列化、配置和跨格式数据交换。
- `serialization::to_variant(any)` / `serialization::to_variant(any_ref)` 负责从 C++ 运行期值转换到数据 DOM。

## 后续规划

- 评估是否把 `any` / `any_ref` 上的 `create_*_view()` 迁移为外部 view 工厂函数，进一步减小类型本身的公开接口。
- 评估轻量 capability / poly 机制，让用户在不修改核心源码的情况下扩展运行期能力。
- 持续补充细粒度 include 边界测试，避免公共头重新形成隐式依赖。
