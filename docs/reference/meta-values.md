<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Meta 值与运行期视图

meta value 子模块负责运行期值访问和数据中间表示，是 `meta` 的一部分，主要包含以下能力：

- `any`：拥有一份 C++ 对象拷贝，保留运行期类型描述。
- `owned_value`：拥有一份由 factory、constructor 或内部构造策略创建出来的对象，可在填充后转成
  `any` 或交给 wrapper 接管。
- `any_ref`：非拥有 C++ 对象引用，用于属性、容器元素和外部对象的零拷贝访问。
- `argument`：反射方法调用参数，能表达左值、const 左值和显式右值参数。
- 各类 view：把指针、wrapper、sequence、set、associative、pair、tuple 和 variant-like 类型按统一接口访问。
- `variant`：脱离精确 C++ 类型的数据 DOM，主要用于序列化、配置和跨格式转换。

旧的 `caors/value*.h` 头文件和 `caors::value::*` 命名空间兼容层已经删除；新代码统一使用
`caors/meta/value*.h` 和 `caors::meta::*`。

## 头文件边界

meta value 提供细粒度头和完整聚合头。细粒度头不再依赖 `any.h`
间接带出其它能力，调用方需要按实际使用显式包含。

| 头文件 | 提供内容 | 适用场景 |
| --- | --- | --- |
| `caors/meta/value/any.h` | `caors::meta::any`、`caors::meta::owned_value`、`caors::any`、`caors::owned_value` | 需要拥有式运行期 C++ 值或高级构造所有权句柄。 |
| `caors/meta/value/any_ref.h` | `caors::meta::any_ref`、`caors::any_ref` | 需要引用外部 C++ 对象或反射属性。 |
| `caors/meta/argument.h` | `caors::meta::argument`、`caors::argument`、`meta::arg()`、`meta::move_arg()` | 需要调用反射方法，尤其是需要表达 `T&&` 参数时。 |
| `caors/meta/value/view_factory.h` | 所有具体 view 类型和显式 `make_*_view()` 工厂 | 高级代码或内部适配层需要显式创建具体 view。 |
| `caors/meta/capability_facade.h` | 类型级 capability facade 基类 | 用 `type_capability_base<Ops>` 或 `type_capability_base<Ops, View>` 定义类型级能力。 |
| `caors/meta/capability_bind.h` | capability 绑定和实例 view helper | 用 `capability_view_base<Ops>`、`capability_bind_traits` 和自动 traits 减少实例 view 样板代码。 |
| `caors/meta/value/capabilities.h` | 类型级 `*_capability` facade 和实例绑定适配 | 需要从 `meta::type` 查询能力，或通过 `any_ref::get_capability<Capability>()` 获取绑定后的 view。 |
| `caors/meta/value/associative_entry_ref.h` | `associative_entry_ref` | 需要按值使用关联容器迭代条目。 |
| `caors/meta/value/variant.h` | `caors::meta::variant`、`caors::variant` | 需要序列化 DOM 或数据中间表示。 |
| `caors/meta/value.h` | meta value 完整公共入口 | 应用层或示例代码需要完整运行期值能力。 |

`any.h` 和 `any_ref.h` 提供 `get_capability<Capability>()` 视图查询入口，但不会提供具体 view 的完整定义。只保存返回值、调用 view 成员函数或按值声明 view 类型时，应额外包含 `caors/meta/value/view_factory.h` 或对应具体 view 头。

## 基本用法

高层动态调用优先使用 `meta::value`。它内联保存常见标量、字符串名、字符串和对象指针；
复杂 C++ 对象再通过 `any` 兜底。反射属性和方法的普通入口都以 `meta::value` 为主：

```cpp
caors::meta::instance instance = caors::meta::view(actor);

instance.set("health", caors::meta::value{100});
caors::meta::value health = instance.get("health");

caors::meta::invoke_value_result result = instance.call("add_health", 5);
```

需要精确 C++ 类型、引用返回或不可自然转换的复杂参数时，再使用 `any`、`any_ref`、
`get_any()`、`set_any()` 和 `call_any()`。

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
#include <caors/meta/value/capabilities.h>

caors::vector<int> numbers{1, 2, 3};
caors::any_ref ref = caors::any_ref::from(numbers);

caors::meta::sequence_view view =
    ref.get_capability<caors::meta::sequence_capability>();
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

`any` 拥有对象副本，适合把属性值从原对象中取出后独立保存。它依赖 `meta::type` 的值操作能力执行默认构造、复制、销毁和赋值。
小型平凡可拷贝、平凡析构的值会直接保存在 `any` 内部，避免为标量和小型 POD 临时值产生堆分配；超过内联容量、
对齐不满足或非平凡析构的类型仍使用 `meta::type` 的普通构造/销毁路径。
`any::default_construct(type_id, memory_resource*)` 可以用指定 `memory_resource` 默认构造对象，返回的
`any` 会记住匹配的资源销毁函数；没有传入资源或类型缺少资源构造函数表时，回退到普通 `new` / `delete`
或小对象内联存储。

资源边界：

- 不传资源时，`any` 会优先内联保存小型平凡对象；不能内联的值使用普通 `new` / `delete` fallback。
- 传入 `memory_resource*` 时，`any` 会优先使用 `meta::type` 的资源构造/销毁能力，不走内联路径，保证分配和释放资源匹配。
- 对 `caors::vector<T>`、`caors::string`、`caors::hash_map<K, V>` 等 allocator-aware 类型，
  资源构造会使用标准 uses-allocator 规则，把同一个 `memory_resource` 传给对象内部 allocator。
- 复制已有值生成 `any` 时会沿用当前存储策略：可内联的小型平凡值继续内联，其他值走 clone；需要指定资源创建空对象再读入数据时，应使用
  `any::default_construct(type_id, resource)`。
- `any::take_ownership(owned_value&&)` 是高级所有权接管入口。它会把 `owned_value`
  中匹配的资源销毁函数或普通销毁函数一起转移到 `any`，保证后续析构仍使用同一套分配 / 释放策略。
  普通业务代码应优先使用 `meta::type::create()` 或 `factory_capability::construct()`，
  不要直接拼接裸指针生命周期。
- `meta::destroy_fn` 和 `meta::resource_destroy_fn` 是高级构造路径共用的销毁函数类型；
  `meta::value_ops`、factory、constructor 和序列化自定义构造都会复用这两个别名。

## owned_value

`owned_value` 是 move-only 的临时所有权句柄，主要服务于“先构造对象，再填充属性或转交给 wrapper”的路径。
它和 `any` 的区别是：`any` 已经是最终拥有式运行期值，而 `owned_value` 允许在接管前访问可写对象地址。

常见来源：

```cpp
caors::meta::factory_capability factory =
    caors::meta::resolve<actor>().get_capability<caors::meta::factory_capability>();

caors::owned_value value = factory.construct_owned(resource);
if (auto* actor_value = value.ref().try_get_mutable<actor>()) {
  actor_value->health = 100;
}

caors::any owned = std::move(value).into_any();
```

需要接管外部高级构造结果时，可以先封装成 `owned_value`：

```cpp
void* raw = construct_actor(resource);
caors::meta::owned_value value =
    caors::meta::make_owned_value(caors::meta::resolve<actor>(), raw, resource, &destroy_actor);
```

规则：

- `owned_value` 析构时会使用匹配的资源销毁函数、自定义销毁函数或 `meta::type` 的销毁能力释放对象。
- `into_any()` 成功后当前 `owned_value` 变为空，所有权和匹配销毁函数一起转移给返回的 `any`。
- `std::move(value).release()` 会释放所有权并返回裸对象、类型、资源和销毁函数，让当前 `owned_value`
  变为空。调用方必须使用返回的 `resource_destroy` 或 `destroy` 手动销毁对象；这是高级接口，普通代码应优先使用
  `into_any()`。
- `released_owned_value_guard` 可以重新托管 `release()` 的返回值，适合把裸对象传给 wrapper、容器或外部系统前的失败回滚。
  目标对象成功接管后调用 `release()` 取回原始结构体，或调用 `dismiss()` 放弃 guard 的销毁责任。
- `make_owned_value(type, value, destroy)` 和 `make_owned_value(type, value, resource, destroy)`
  是高级 RAII 包装入口；调用方必须保证 `type`、原始对象、销毁函数和资源完全匹配。
- 公开 facade 只暴露 `construct()` / `construct_owned()` 这类 RAII 入口；裸 `void*`
  构造和销毁函数只作为底层 ops/model 的内部实现细节。

所有权接口分层约定：

- 公开 facade 面向普通用户，只返回 `any`、`owned_value`、`any_ref` 或具体 view，不暴露 `void*`。
- `owned_value` 是“构造后尚未进入最终容器”的 RAII 句柄；`any` 是最终拥有式运行期值。
- `released_owned_value` / `released_owned_value_guard` 是高级桥接工具，仅用于 wrapper 接管、容器 emplace
  或序列化内部构造这类需要短暂暴露裸对象的路径。
- 底层 `*_ops` / model 可以使用 `void*`，但必须把匹配销毁函数向上交给 `owned_value` 或 `any`，
  不能让分配和释放策略分离。

当前限制：

- 不可拷贝类型无法被复制成有效 `any`。
- `any` 不保留对象身份、共享关系或对象图引用。
- 未注册 C++ 类型仍可通过 `meta::resolve<T>()` 返回的 `meta::type` 使用基础值能力，但没有稳定名称、GUID、反射属性或多态构造信息。

## any_ref

`any_ref` 不拥有对象，只保存对象地址、运行期类型信息和可写性。调用方必须保证被引用对象在 `any_ref` 使用期间仍然存活。

常见来源：

- `caors::any_ref::from(value)`：引用普通 C++ 对象。
- `caors::any_ref::from(runtime_type, pointer)`：显式传入运行期类型，并校验其与指针静态类型的
  ID、大小和对齐；可写性由指针的 const 性自动推导。
- `meta::property::get_ref(object)`：引用反射属性。
- view 的 `element_ref()`、`value_ref()`、`first_ref()` 等：引用容器元素或结构内部值。

`any_ref` 可以写回同类型值；只读引用、类型不匹配或底层类型不支持赋值时返回失败。
完全擦除的 `meta::type + void*` 构造入口仅供 any、property、view 和 serialization 内部使用。

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
    ref.get_capability<caors::meta::sequence_capability>();
caors::meta::indirect_view indirect =
    ref.get_capability<caors::meta::indirect_capability>();
```

### 高级用法：显式 view 工厂

`caors::meta::make_sequence_view()`、`make_indirect_view()` 等 `make_*_view()` 函数是显式 view
工厂，主要服务 capability 绑定、测试边界和高级适配代码。普通用户应优先使用
`any_ref::get_capability<Capability>()`，这样类型级和实例级查询都使用同一个 capability 入口。
直接把具体 view 类型作为 `get_capability` 模板参数的旧查询方式不再保留。

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

这些 view 通过 `meta::type` 暴露的类型能力工作，不要求容器类型本身一定注册到全局 meta
registry。`get_*_type()` 系列接口返回 `meta::type`；调用方需要 `type_id` 或名称时，
直接读取返回视图的 `id()` / `name()`。需要稳定名称、GUID、反射属性、序列化策略或多态构造的业务类型仍应显式注册。

集合和关联容器的 erased iterator 是 meta value 内部临时对象，使用 CAORS 内部资源策略分配，并在 iterator
视图析构时自动释放。它不使用用户容器的 allocator，也不会延长底层容器元素生命周期。

## variant

`variant` 是数据 DOM，不保留精确 C++ 类型，只表达 null、bool、整数、浮点、字符串、数组、字典和对象等格式友好的数据形态。

它和 `any` 的边界：

- `any` 面向 C++ 类型和反射属性访问。
- `variant` 面向序列化、配置和跨格式数据交换。
- `serialization::to_variant(any)` / `serialization::to_variant(any_ref)` 负责从 C++ 运行期值转换到数据 DOM。

## 后续规划

数学值快路径的候选范围见[统一路线图](../roadmap.md#05-math-基础数学类型)，
测量方法见[Benchmark 指南](../guides/benchmarking.md#math-fast-path-判断)。本页只维护当前值类型契约；
原规划说明保留在[整理前基线](../records/history/2026-10-19-roadmap-baseline.md)。
