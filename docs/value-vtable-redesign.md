<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Meta value vtable 与 any_ref 边界重构计划

## 背景

当前 `type_record` 直接保存 `value_storage_ops`，`any`、`any_ref`、`owned_value`、属性访问、
运行期容器 view 和序列化流程会通过 `meta::type` 反复查询这组操作。这样虽然支持
`meta::type + void*` 的完全运行期值操作，但也造成以下问题：

- 值实例的复制、赋值和销毁行为依赖类型记录查询，没有内聚到值对象自身；
- `any_ref::from_raw(meta::type, const void*, bool)` 同时承担静态类型和擦除类型入口，
  调用方可以错误组合类型、地址与可写性；
- `value_storage_ops` 既承担值实例 vtable，又承担运行期默认创建 fallback，职责偏宽；
- 静态已知 `T` 的路径没有利用模板信息直接校验类型和推导 const 性。

本计划将值实例操作收敛为由 `any` 和 `any_ref` 持有的轻量 fake vtable，同时在
`type_record` 中保留一个最小 provider，使序列化、容器 view 等完全擦除路径仍可由
`meta::type` 获得对应 vtable。

## 目标

- `any`、`any_ref` 和 `owned_value` 在创建时绑定 `const value_vtable*`；
- 后续值复制、赋值、析构和布局查询优先使用实例已经绑定的 vtable；
- 静态类型入口通过模板生成 vtable，并验证显式 `meta::type` 与 `T` 一致；
- `const T*` 自动生成只读 `any_ref`，不再由外部传入可写标志；
- `type_record` 只保留获取 value vtable 的函数指针，不保存一份重复 vtable；
- 保留私有擦除入口，服务只有 `meta::type + void*` 的内部运行期流程；
- 将动态创建与普通值实例生命周期解耦，创建时仍先查询 factory/constructor。

## 非目标

- 不改变 `any`、`any_ref` 的所有权语义；
- 不要求用户为了使用内建 codec 或普通静态能力手动注册类型；
- 不改变属性、方法、容器 view 和序列化的公开行为；
- 不在本轮设计 DLL 卸载协议；
- 不把全部 capability 合并进 value vtable；
- 不允许仅凭 `T*` 自动推断其多态动态派生类型。

## 目标结构

```text
静态类型路径
T / T*
  └─ value_vtable_of<T>()
       └─ any / any_ref 保存 const value_vtable*

运行期擦除路径
meta::type + void*
  └─ type_record::get_value_vtable()
       └─ value_vtable provider
            └─ any_ref::from_erased(...)

动态创建路径
meta::type
  ├─ factory capability
  ├─ constructor capability
  └─ type default-construction fallback
       └─ 创建 any，并绑定同一 value_vtable
```

## 核心设计

### value_vtable

`value_vtable` 表达一个已经存在的 C++ 值所需的最小类型擦除协议。初步包含：

```cpp
struct value_vtable {
  type_layout layout{};
  value_traits traits{};
  value_ops::clone_fn clone = nullptr;
  value_ops::assign_fn assign = nullptr;
  value_ops::destroy_fn destroy = nullptr;
  value_ops::resource_destroy_fn resource_destroy = nullptr;
};
```

最终字段应以 `any`、`any_ref` 和 `owned_value` 的实际调用需求为准。默认构造和带参构造不属于
“已存在值”的实例行为，应继续由创建路径负责，避免为了动态创建把 factory/constructor
职责重新塞回 value vtable。

### type_record provider

`type_record` 保存一个最小获取函数：

```cpp
using get_value_vtable_fn = const value_vtable* (*)() noexcept;
```

静态 `type_record<T>` 与注册后的 `type_record` 都携带同一个 provider。provider 返回地址稳定的
模板静态 vtable：

```cpp
template <typename T>
[[nodiscard]] const value_vtable* value_vtable_of() noexcept;
```

provider 是完全擦除路径的桥梁，不是实例操作的长期查询入口。`any` 或 `any_ref` 获得 vtable 后
应自行保存。

### any_ref 入口

公开静态类型路径应优先使用已有 `from(T&)`，并增加需要显式类型校验的模板入口：

```cpp
template <typename T>
[[nodiscard]] static any_ref from(meta::type type, T* data) noexcept;
```

该入口负责：

- 去除 `T` 的 cv 修饰后获得实际值类型；
- 校验 `type.id()`、大小和对齐是否与 `resolve<T>()` 一致；
- 从 `T` 的 const 性自动推导引用是否可写；
- 直接绑定 `value_vtable_of<T>()`；
- 空地址或类型不匹配时返回无效引用。

真正擦除的内部路径保留私有入口：

```cpp
[[nodiscard]] static any_ref from_erased(
    meta::type type,
    const value_vtable* vtable,
    const void* data,
    bool writable) noexcept;
```

如果调用方只有 `meta::type + void*`，由 `type_record` provider 取得 vtable 后进入该函数。
`from_erased` 仅通过 friend 或内部 access 类开放，不继续作为普通公共 API。

### 动态创建 fallback

动态创建遵循以下优先级：

1. 已注册 factory capability；
2. 已注册 constructor capability；
3. 配置允许时使用类型默认构造 fallback；
4. 均不可用时返回创建失败。

默认构造 fallback 应保持轻量，可由 `type_record` 保存独立创建函数，或由 provider 返回的内部
创建描述获得。实施前根据现有 `factory_ops`、`constructor_ops` 和
`allow_type_default_construction` 调用链确定最终位置，不让 value vtable 承担注册构造重载。

## 当前调用点分类

### 可迁移到模板入口

- `any_ref::from(T&)`、`from_const(const T&)` 及带注册名称的重载；
- `property_model.h` 中已知 `value_type` 或 `property_type` 的 getter/setter model。

这些路径应直接绑定 `value_vtable_of<T>()`，并移除手写 `writable` 参数。

### 必须保留擦除入口

- `any::ref()` 和 `owned_value::ref()`；
- `sequence_view`、`set_view`、`associative_view`、`pair_view`、`tuple_view`、
  `variant_view` 和间接访问 view；
- `property_record` 中只保存运行期属性类型的回退路径；
- serialization 的字段 schema、对象地址和动态值分派路径。

这些调用点已经丢失原始 `T`，不能通过增加模板参数恢复类型。迁移后应传递上游已绑定的
vtable，或通过 `meta::type` 的 provider 获取一次。

## 分阶段实施

### 阶段 1：建立 value_vtable

- 盘点 `any`、`any_ref`、`owned_value` 对 `value_storage_ops` 的全部使用；
- 确定 value vtable 的最小字段集合；
- 增加 `value_vtable_of<T>()` 模板 model；
- 为可拷贝、不可拷贝、可赋值、不可赋值、平凡和非平凡类型补充单元测试。

完成标准：模板 vtable 能独立描述已有值的布局、复制、赋值和销毁行为。

### 阶段 2：增加 type_record provider

- 在 `type_record` 中加入 `get_value_vtable_fn`；
- 静态类型记录构建时绑定对应 `T` 的 provider；
- pending/commit/merge 复制固有 provider，并验证同一类型 provider 的一致性；
- 让 `meta::type` 提供仅供内部使用的安全查询入口；
- 保持静态 fallback 和注册记录行为一致。

完成标准：已注册与未注册类型都能从 `meta::type` 获得地址稳定的 value vtable。

### 阶段 3：迁移 any 与 owned_value

- 让 `any`、`any_ref`、`owned_value` 保存 `const value_vtable*`；
- 创建、复制、移动、赋值和销毁流程使用实例 vtable；
- 动态创建成功后立即绑定对应 vtable；
- 保留必要的内联存储判断和 memory resource 销毁路径；
- 移除这些值对象对 `type.get_raw_ops<value_storage_ops>()` 的重复查询。

完成标准：值实例完成绑定后，其生命周期操作不再依赖 registry 查询。

### 阶段 4：收敛 any_ref 创建 API

- 让 `from(T&)` 和 `from_const(const T&)` 直接走模板绑定路径；
- 增加显式 `meta::type + T*` 的类型安全模板入口；
- 将现有公共 `from_raw(meta::type, const void*, bool)` 改为私有 `from_erased`；
- 将 `property_model` 的已知类型调用迁移到模板入口；
- 将 const 性推导和类型不匹配行为写入公共 API 注释与测试。

完成标准：普通调用者不能自行组合错误的类型、地址和 writable 标志。

### 阶段 5：迁移运行期擦除调用点

- `any` 和 `owned_value` 向 `any_ref` 直接传递自身 vtable；
- 各类容器 view 在构建时绑定元素相关 vtable，或在首次访问时通过 provider 获取；
- `property_record` 和序列化路径统一使用私有 erased bridge；
- 避免循环中重复通过 `meta::type` 查询相同 vtable；
- 删除不再使用的 `from_raw` 和 value ops 查询 helper。

完成标准：所有擦除调用点都明确标注其 vtable 来源和 writable 来源。

### 阶段 6：拆分创建职责

- 将默认创建 fallback 与值实例 vtable 分离；
- 保持 factory/constructor 优先级；
- 评估 `value_storage_ops` 是否可以删除，或缩减为兼容期内部聚合；
- 清理 `value_storage_ops` 作为静态 capability 的特殊查询分支；
- 更新 meta、value、registration 和 serialization 文档。

完成标准：`type_record` 不再直接保存宽泛的 `value_storage_ops`，只保存运行期必要 provider
和独立创建 fallback。

## 验证计划

- `any`：空值、内联值、堆值、复制、移动、赋值、清空和自赋值；
- `any_ref`：可写、只读、const 推导、类型不匹配、空地址和 `to_any()`；
- 类型特征：不可默认构造、不可复制、不可赋值、非平凡析构和过对齐类型；
- registry：未注册 fallback、注册后重新 resolve、重复注册和 commit 失败回滚；
- 属性：成员字段、访问器、全局变量、const getter 和只读属性；
- view：sequence、set、associative、pair、tuple、variant 和 indirect；
- serialization：字段读写、多态对象、codec、默认构造开关和错误回滚；
- 跨翻译单元：同一类型的 vtable 行为一致，不以 vtable 地址作为类型身份；
- Windows MSVC Debug 单线程构建与测试；
- Windows Clang Debug 单线程构建与测试。

所有构建使用 `--parallel 1`，降低当前环境发生 OOM 的风险。

## 风险与约束

- vtable 地址只用于调用，不作为 `type_id` 或 C++ 类型相等判断依据；
- provider、vtable 和其函数地址依赖定义它们的模块生命周期，模块卸载前必须清理相关类型记录和值；
- 注册记录复制 provider 时不能把不同 C++ 类型的 provider 合并到同一 `type_id`；
- 模板入口中的 `T*` 只描述静态 `T`，不会把基类指针自动提升为派生类型；
- `any_ref` 不拥有对象，增加 vtable 后也不改变原对象生命周期要求；
- 迁移期间需要避免同时使用实例 vtable和 `type_record` ops 而产生两套事实源。

## 开放问题

- `value_vtable` 是否保留普通 `destroy`，还是统一使用 resource-aware destroy；
- 内联存储判断所需 traits 是否直接放在 vtable；
- 容器 view 应在 view 构建时缓存元素 vtable，还是由每个返回的 `any_ref` 查询 provider；
- 默认创建 fallback 最终放在 `type_record` 的独立函数指针还是单独的 creation provider；
- 兼容期是否保留内部 `value_storage_ops` 聚合，以及删除它的具体版本节点。

