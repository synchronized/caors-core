<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Meta Value

meta value 子模块负责运行期值访问和数据中间表示，是 `meta` 的一部分，主要包含四类能力：

- `any`：拥有一份 C++ 对象拷贝，保留运行期类型描述。
- `any_ref`：非拥有 C++ 对象引用，用于字段、容器元素和外部对象的零拷贝访问。
- 各类 view：把指针、wrapper、sequence、set、associative、pair、tuple 和 variant-like 类型按统一接口访问。
- `variant`：脱离精确 C++ 类型的数据 DOM，主要用于序列化、配置和跨格式转换。

旧的 `caors/value*.h` 头文件和 `caors::value::*` 命名空间兼容层已经删除；新代码统一使用
`caors/meta/value*.h` 和 `caors::meta::*`。

## 头文件边界

meta value 提供细粒度头和完整聚合头。细粒度头不再依赖 `any.h`
间接带出其它能力，调用方需要按实际使用显式包含。

| 头文件 | 提供内容 | 适用场景 |
| --- | --- | --- |
| `caors/meta/value/any.h` | `caors::meta::any`、`caors::any` | 只需要拥有式运行期 C++ 值。 |
| `caors/meta/value/any_ref.h` | `caors::meta::any_ref`、`caors::any_ref` | 需要引用外部 C++ 对象或反射字段。 |
| `caors/meta/value/view_factory.h` | 所有具体 view 类型和显式 `make_*_view()` 工厂 | 高级代码或内部适配层需要显式创建具体 view。 |
| `caors/meta/value/capability_facade.h` | 自定义 capability facade/view 的轻量 helper | 用 `type_capability_base<Ops>`、`capability_view_base<Ops>` 和自动 traits 减少样板代码。 |
| `caors/meta/value/capability_view.h` | `any` / `any_ref` 的 `get_capability<View>()` 视图绑定适配 | 推荐的统一运行期视图查询入口。 |
| `caors/meta/value/type_capability.h` | 类型级 `*_capability` facade | 需要先从 `meta::type` 查询能力，再绑定到具体 `any_ref`。 |
| `caors/meta/value/associative_entry_ref.h` | `associative_entry_ref` | 需要按值使用关联容器迭代条目。 |
| `caors/meta/value/variant.h` | `caors::meta::variant`、`caors::variant` | 需要序列化 DOM 或数据中间表示。 |
| `caors/meta/value.h` | meta value 完整公共入口 | 应用层或示例代码需要完整运行期值能力。 |

`any.h` 和 `any_ref.h` 提供 `get_capability<View>()` 视图查询入口，但不会提供具体 view 的完整定义。只保存返回值、调用 view 成员函数或按值声明 view 类型时，应额外包含 `caors/meta/value/view_factory.h` 或对应具体 view 头。

## 基本用法

只使用拥有式值：

```cpp
#include <caors/meta/value/any.h>

caors::any value = caors::any::from(42);
const int* stored = value.try_get<int>();
```

引用外部对象：

```cpp
#include <caors/meta/value/any_ref.h>

int number = 1;
caors::any_ref ref = caors::any_ref::from(number);
ref.try_set(2);
```

创建运行期视图：

```cpp
#include <caors/core/containers.h>
#include <caors/meta/value/any_ref.h>
#include <caors/meta/value/capability_view.h>

caors::vector<int> numbers{1, 2, 3};
caors::any_ref ref = caors::any_ref::from(numbers);

caors::meta::sequence_view view =
    ref.get_capability<caors::meta::sequence_view>();
for (caors::any_ref element : view) {
  element.try_set(0);
}
```

应用层如果不关心编译边界，可以直接使用完整入口：

```cpp
#include <caors/meta/value.h>

caors::variant data = caors::variant::object({});
caors::any value = caors::any::from(7);
```

## any

`any` 拥有对象副本，适合把字段值从原对象中取出后独立保存。它依赖 meta descriptor 执行默认构造、复制、销毁和赋值。
小型平凡可拷贝、平凡析构的值会直接保存在 `any` 内部，避免为标量和小型 POD 临时值产生堆分配；超过内联容量、
对齐不满足或非平凡析构的类型仍使用 descriptor 的普通构造/销毁路径。
`any::default_construct(type_id, memory_resource*)` 可以用指定 `memory_resource` 默认构造对象，返回的
`any` 会记住匹配的资源销毁函数；没有传入资源或类型缺少资源构造函数表时，回退到普通 `new` / `delete`
或小对象内联存储。

资源边界：

- 不传资源时，`any` 会优先内联保存小型平凡对象；不能内联的值使用普通 `new` / `delete` fallback。
- 传入 `memory_resource*` 时，`any` 会优先使用 descriptor 中的资源构造/销毁函数，不走内联路径，保证分配和释放资源匹配。
- 对 `caors::vector<T>`、`caors::string`、`caors::hash_map<K, V>` 等 allocator-aware 类型，
  资源构造会使用标准 uses-allocator 规则，把同一个 `memory_resource` 传给对象内部 allocator。
- 复制已有值生成 `any` 时会沿用当前存储策略：可内联的小型平凡值继续内联，其他值走 clone；需要指定资源创建空对象再读入数据时，应使用
  `any::default_construct(type_id, resource)`。
- `any::take_ownership(type, value, resource, destroy)` 是高级所有权接管入口。调用方必须保证 `value`
  指向与 `type` 匹配的对象，并且 `destroy` 与构造策略和 `resource` 配套。普通业务代码应优先使用
  `runtime::type::create()`，不要直接拼接裸指针生命周期。

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

推荐通过 `any` / `any_ref` 的统一能力入口创建运行期视图：

```cpp
caors::meta::sequence_view sequence =
    ref.get_capability<caors::meta::sequence_view>();
caors::meta::indirect_view indirect =
    ref.get_capability<caors::meta::indirect_view>();
```

### 高级用法：显式 view 工厂

`caors::meta::make_sequence_view()`、`make_indirect_view()` 等 `make_*_view()` 函数是显式 view
工厂，主要服务 capability 绑定、测试边界、自定义 `capability_view_traits` 和高级适配代码。普通用户应优先使用
`any_ref::get_capability<View>()`，这样调用点不需要直接关心底层 descriptor 和 source 绑定细节。

如果调用方先拿到的是类型视图，可以使用类型级 `*_capability` facade，再把它绑定到具体实例：

```cpp
#include <caors/adapters/stl_traits.h>
#include <caors/meta/value.h>

#include <vector>

std::vector<int> values{1, 2, 3};

caors::meta::sequence_capability sequence =
    caors::meta::resolve<std::vector<int>>()
        .get_capability<caors::meta::sequence_capability>();

caors::meta::sequence_view view =
    sequence.bind(caors::meta::any_ref::from(values));
```

类型级 facade 不拥有类型数据，也不拥有实例。它只是把 `meta::type`
和内部函数表包装成更适合公开 API 使用的对象；实例访问仍由绑定后的 `*_view` 完成。

这些 view 通过 meta 内部 descriptor 保存的类型能力工作，不要求容器类型本身一定注册到全局 meta
registry。`get_*_type()` 系列接口返回 descriptor-backed `meta::type`；调用方需要 `type_id` 或名称时，
直接读取返回视图的 `id()` / `name()`。需要稳定名称、GUID、反射字段、序列化策略或多态构造的业务类型仍应显式注册。

集合和关联容器的 erased iterator 是 meta value 内部临时对象，使用 CAORS 内部资源策略分配，并在 iterator
视图析构时自动释放。它不使用用户容器的 allocator，也不会延长底层容器元素生命周期。

## variant

`variant` 是数据 DOM，不保留精确 C++ 类型，只表达 null、bool、整数、浮点、字符串、数组、字典和对象等格式友好的数据形态。

它和 `any` 的边界：

- `any` 面向 C++ 类型和反射字段访问。
- `variant` 面向序列化、配置和跨格式数据交换。
- `serialization::to_variant(any)` / `serialization::to_variant(any_ref)` 负责从 C++ 运行期值转换到数据 DOM。

## 后续规划

- 持续补充细粒度 include 边界测试，避免公共头重新形成隐式依赖。
