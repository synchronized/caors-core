<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Meta 擦除 wrapper 迁移计划

## 文档状态

- 当前状态：property、method record 边界迁移与边界复审已完成；性能复验待补充；
- 已完成范围：property、method 注册模型与运行时记录边界；
- 后续范围：补充尺寸、分配与访问 benchmark；constructor 与 type model 仅在出现实际需求时再评估；
- 性能复验：[meta-wrapper-performance-validation.md](meta-wrapper-performance-validation.md)；
- 设计取代：本计划取代
  [meta-wrapper-model-redesign.md](meta-wrapper-model-redesign.md)
  中“继续将模板状态和固定 ops table 内联在 record 内”的后续演进方向；旧文档仍保留为已完成重构的历史依据。

## 背景

CAORS 当前采用编译 runtime：

- 用户头文件中的模板根据具体 C++ 类型生成 property、method 和 builtin model；
- CAORS runtime 统一拥有 registry、默认 context、memory resource 和运行时记录；
- 公共 `meta::property`、`meta::method` 和 `meta::type` 是轻量只读句柄。

但当前 `property_record`、`method_record` 同时保存通用注册信息、固定 ops table 和内联模板状态。
注册模块与 CAORS runtime 因此必须共同理解 record 布局。该结构接近 EnTT 的 header-only node，
却承担了 RTTR 式编译 runtime 的 ABI 边界。

本计划采用 RTTR 路线的职责分层，但不复制其虚基类体系：

```text
模板注册层（头文件）
└─ 生成具体 wrapper/model

CAORS runtime
├─ 拥有 registry 与 record
├─ 保存 opaque wrapper
└─ 通过统一 request dispatch 调用 wrapper

公共 handle
└─ 只引用 runtime record
```

CAORS 不在本计划中转为 header-only。默认 context、registry 和 library resource 继续由唯一 runtime
提供，避免 EXE 与不同 DLL 各自持有默认全局状态。

## 命名决定

不单独引入 `property_vtable`、`method_vtable` 等类型，避免把简单擦除协议拆得过碎。

完整擦除对象统一称为 wrapper：

```cpp
struct property_wrapper {
  void* state = nullptr;
  property_invoke_fn invoke = nullptr;
  property_destroy_fn destroy = nullptr;
  memory::memory_resource* resource = nullptr;
};
```

各名称职责如下：

- `property_record`：名称、schema、metadata 等 runtime 注册数据；
- `property_wrapper`：具体 property 模型的擦除状态、调用入口和所有权；
- `property_model`：模板生成的成员、accessor 或全局属性实现；
- `property_request_kind`：一次 property 操作的稳定编号；
- `request_payload`：只在一次同步调用期间传递请求对象地址。

`invoke` 和 `destroy` 已经构成 wrapper 的最小 fake-vtable，不再额外包装成 `*_vtable`。

## 通用 request 协议

### request_payload

`request_payload` 只擦除请求对象地址，不保存类型、所有权或生命周期：

```cpp
class request_payload {
public:
  constexpr request_payload() noexcept = default;

  template <typename T>
  explicit request_payload(T& value) noexcept
      : data_(std::addressof(value)) {}

  template <typename T>
  [[nodiscard]] T& get() const noexcept {
    return *static_cast<T*>(data_);
  }

private:
  void* data_ = nullptr;
};
```

约束：

- 只接受左值请求对象，不提供持有临时对象的构造；
- wrapper 不得保存 `request_payload` 或其中地址；
- `request_kind` 与请求结构必须一一对应；
- 公共 API 不直接暴露通用 dispatch，只暴露强类型包装函数；
- 未知 request kind 返回失败，不读取 payload；
- request kind 使用固定宽度底层类型，已发布数值只追加、不复用。

### 独立请求域

不定义包含所有 meta 操作的巨型全局枚举。各模型使用独立请求域：

```cpp
enum class property_request_kind : std::uint32_t;
enum class method_request_kind : std::uint32_t;
enum class constructor_request_kind : std::uint32_t;
enum class type_request_kind : std::uint32_t;
```

这样统一调用形式，同时保留 property、method、constructor 和 type 的语义边界。

## Property 当前结构

当前 `property_record` 直接保存：

```text
property_record
├─ name_
├─ storage_[4 * sizeof(void*)]
├─ offset_
├─ writable_
├─ property_storage_kind
├─ property_ops（8 个函数指针）
├─ schema_
└─ metadata_
```

不同模型的状态为：

- member：保存成员偏移；
- accessor：在 `storage_` 中按字节保存 getter/setter 成员函数指针；
- global：在 `storage_` 中保存全局对象指针。

当前 accessor 状态必须可平凡复制，且总大小不得超过四个指针。

## Property 目标结构

迁移后：

```text
meta::property
└─ const property_record*
   ├─ name_
   ├─ schema_
   ├─ metadata_
   └─ property_wrapper
      ├─ state
      ├─ invoke
      ├─ destroy
      └─ resource
```

`property_record` 不再直接保存：

- `storage_`；
- `offset_`；
- `writable_`；
- `property_storage_kind`；
- `property_ops`；
- `load_pointer()` / `store_pointer()`。

这些内容由具体 wrapper/model 解释。`property_record` 仍提供强类型薄封装，使 `property.cpp`
无需直接构造 request：

```cpp
meta::type property_record::get_type() const noexcept;
any property_record::get_any(const void* object) const;
any_ref property_record::get_ref(const void* object) const;
any_ref property_record::get_ref(void* object) const;
bool property_record::try_set(void* object, const any& source) const;
```

公共 `meta::property` API、注册语法和序列化格式保持不变。

## Property 模型

### 成员属性

成员属性优先使用非类型模板参数模型：

```cpp
template <auto Member>
struct member_property_model;
```

成员指针编码在模板实例中时，wrapper 可以无状态：

```text
state = nullptr
invoke = member_property_model<Member>::invoke
destroy = nullptr
```

模型负责 owner/value type、读写性、成员地址、`any`/`any_ref` 读取和赋值请求。

### Accessor 属性

getter/setter 模型保存具体 callable：

```cpp
template <typename Class, typename Getter, typename Setter>
struct accessor_property_model {
  Getter getter;
  Setter setter;
};
```

迁移后不再受四个指针内联存储和 trivially-copyable 限制，可以支持符合注册约束的有状态
callable。只读 accessor 使用独立模型或空 setter 表达，不在 record 中增加分支状态。

### 全局属性

全局属性模型保存被访问对象指针：

```cpp
template <typename Property>
struct global_property_model {
  Property* value;
};
```

global property 与 member/accessor 使用相同 wrapper 协议，不再由 `property_record` 解释
`storage_`。

## Property 请求

首版请求至少覆盖：

```text
owner_type
value_type
storage_info
const_data
mutable_data
get_any
get_const_ref
get_mutable_ref
set_any
```

每项使用明确结构，例如：

```cpp
struct property_get_any_request {
  const void* object = nullptr;
  any result{};
};

struct property_set_any_request {
  void* object = nullptr;
  const any* source = nullptr;
  bool result = false;
};

struct property_storage_info {
  property_storage_kind kind = property_storage_kind::none;
  std::size_t offset = 0;
  meta::type value_type{};
  bool writable = false;
};
```

是否保留 `const_data` / `mutable_data` 由实施前调用点盘点决定；如果只被 compiled accessor 使用，
则只保留 `storage_info` 和高层值访问请求，避免公开无必要的裸地址操作。

## compiled property accessor

wrapper 迁移不能让普通成员字段热路径退化。

`compile_property_accessor()` 在编译 accessor 时查询一次 `storage_info`：

- member 属性缓存 offset、value type 和 writable，后续继续直接执行地址偏移；
- accessor/global 属性缓存 wrapper 的 `state + invoke` 或回退到 `property_record`；
- compiled accessor 的有效期不得超过所属 registry record 和注册模块。

迁移前后需要对 property 直接访问与 compiled accessor 做专项基准比较。

## 所有权和 DLL 生命周期

`destroy` 销毁的是有状态 wrapper model，不是被反射对象或 property 值。

规则：

- 无状态模型允许 `state == nullptr` 且 `destroy == nullptr`；
- 有状态模型使用显式 `memory_resource` 分配；
- `destroy` 必须在生成具体 model 的模块中执行具体析构，再通过同一 resource 释放内存；
- `property_record` 默认改为 move-only，registry commit 转移 wrapper 所有权；
- 若现有事务流程确实要求复制，再为 wrapper 增加 `clone`，不提前引入；
- 注册模块卸载前必须保证其 wrapper 已从 registry 清除或 registry 已销毁；
- wrapper 所在模块卸载后，`invoke` 和 `destroy` 都会悬空，request dispatch 不能解决模块卸载问题。

## Method 后续迁移

property 方案稳定后，复用相同基础设施迁移 `method_record`：

```text
method_record
├─ name
├─ metadata
├─ parameter_names
├─ default_arguments
└─ method_wrapper
   ├─ state
   ├─ invoke
   ├─ destroy
   └─ resource
```

以下通用注册数据继续留在 record：

- 方法名与 metadata；
- 参数名；
- 默认参数；
- 重载集合关系。

具体 callable、owner/return/parameter type 解析、参数 rank/prepare 和调用行为进入 method model。
请求至少覆盖：

```text
owner_type
return_type
parameter_count
parameter_type
rank_arguments
prepare_arguments
invoke
invoke_value
```

`method_set_record` 继续负责重载集合、歧义判断和公共调用编排，不为集合增加模板 wrapper。

实施结果：

- `method_record` 已改为 move-only，不再直接保存成员函数指针 storage、callable storage 或
  `method_ops` 字段；
- 新增 move-only `method_wrapper` 和独立 `method_request_kind`，owner/return/parameter
  查询、参数 rank/prepare、argument/value 调用统一通过 request dispatch；
- 默认参数、参数名、metadata 和重载集合继续由 runtime record 管理；
- member、const member、global function 和有状态 global callable 继续复用已验证的模板
  model 算法；model 直接处理 request，固定 `method_ops` 已删除；
- `method_wrapper` 仅保存擦除后的 model state、单个 `invoke` 入口和销毁职责，
  不再保存第二层固定函数表；
- Windows Clang Debug 单线程完成 cross-module、method、property、registry、registry-view、
  value、serialization-core 和公共头文件自包含构建，相关测试全部通过。

## Constructor 与 type model

constructor 暂不随 property 第一阶段迁移。property 和 method 完成后重新确认：

- 如果 constructor 仍只是一组类型 capability，继续保留 `constructor_ops`；
- 只有存在多个独立构造函数节点、参数签名、metadata 或重载集合时，才引入
  `constructor_record + constructor_wrapper`。

`type_record` 本身不 wrapper 化。它继续由 runtime 直接拥有和解释：

- 类型身份、名称、GUID、布局；
- property、method、nested；
- metadata、extension 和注册 capability；
- 父类型关系。

模板生成的静态类型行为未来可以单独抽为 `type_model`，但不属于 property 第一阶段。

## 暂不迁移

第一阶段不迁移：

- `value_storage_ops`；
- scalar、enum、indirect、sequence、set、associative、pair、tuple、variant builtin ops；
- `ptr_ops`、`handle_ops`、`codec_ops`；
- 用户自定义 capability；
- `enum_value_record`；
- `any` / `any_ref` 的值存储模型；
- registry、context、memory resource 的 runtime 所有权。

这些对象已经是独立函数表或纯运行时数据，没有必要为了形式统一全部改成 wrapper。

## 实施阶段

### 阶段 0：基线与约束

- 记录 `property_record` 大小、property 注册分配次数和 compiled accessor 基准；
- 盘点 `property_record` 所有复制、移动和跨模块入口；
- 确认插件卸载与 registry 生命周期约束；
- 为 request kind 固定数值和失败语义。

### 阶段 1：通用擦除基础设施

- 新增 `request_payload`；
- 新增 move-only owned wrapper 辅助设施；
- 覆盖无状态、有状态、移动、销毁和未知请求测试；
- 不在公共 namespace 暴露直接 dispatch API。

### 阶段 2：Property wrapper

- 迁移 member、accessor 和 global property model；
- 删除 `property_ops` 与 `property_record::storage_`；
- 保持公共 property API 和注册语法；
- 保持 metadata/schema 行为；
- 更新 pending/commit/module property 所有权链路。

实施结果：

- 新增非拥有 `request_payload` 和 move-only `property_wrapper`；
- `property_record` 已删除固定 `property_ops`、四指针内联 storage、offset、kind 和 writable
  状态，只保留名称、schema、metadata 与 opaque wrapper；
- member、accessor 和 global property 已统一为
  `state + invoke + destroy + resource` 模型；
- property 记录在 pending、type 和 module 链路中按移动传递；
- 公共 `meta::property` API、注册语法和序列化格式未改变。

现有成员属性注册接口接收运行期成员指针，member model 因此仍需保存提交期计算的 offset，
当前由 wrapper 通过 library resource 持有。未来若增加 `property<&T::member>()` 非类型模板参数
入口，可进一步生成无状态 member wrapper；该优化不阻塞本轮 ABI 边界迁移。

### 阶段 3：Property 热路径与验证

- 迁移 `compile_property_accessor()`；
- 验证成员、只读 accessor、可写 accessor 和 global property；
- 验证精确 any、高层 value、const 与 writable 语义；
- 验证 serialization property 路径；
- 对比迁移前后尺寸、分配和访问性能。

实施结果：

- compiled member accessor 在编译时缓存 offset、value type 和 writable，读取与写入不经过
  wrapper dispatch；
- accessor/global compiled accessor 继续通过稳定 `property_record` 转发；
- Windows Clang Debug 使用单线程完成 cross-module、property、registry、registry-view、
  value 和 serialization-core 构建；
- 上述 6 项测试及公共头文件自包含测试全部通过；
- property 尺寸与分配 benchmark 尚未补充，作为后续性能复验项保留。

### 阶段 4：Method wrapper

- 根据 property 实施结果复用基础设施；
- 迁移 member/global callable；
- 保留默认参数、命名参数和重载集合在 runtime record；
- 完整复测重载、转换、右值、默认参数和命名参数。

### 阶段 5：边界复审

- 决定 constructor 是否需要独立 wrapper；
- 决定 type model 是否需要统一 request dispatch；
- 重新评估 `find_type_builtin_ops`，但不默认迁移高频 builtin ops；
- 更新 meta 架构文档和 DLL 使用约束。

复审结论：

- `constructor_ops` 继续作为类型 capability；当前没有独立构造函数节点、名称、metadata
  或重载集合，不引入 `constructor_record + constructor_wrapper`；
- `type_record` 继续作为 runtime 聚合根，不增加统一 `type_request_kind` dispatch；
- 删除 `builtin_ops_kind` 与 `find_type_builtin_ops` 的双向类型映射；
  `type::get_raw_ops<Ops>()` 直接把 `capability_key_of<Ops>()` 传给 runtime 的
  `find_type_static_capability()`；
- builtin ops、静态 capability 和 registry 所有权结构保持不变。

## 验收标准

- 公共 `meta::property` API 和现有注册语法兼容；
- `property_record` 不再包含模板状态内联 storage 和固定 `property_ops`；
- runtime 不需要理解 getter/setter/member/global model 的具体 C++ 类型；
- 无状态 member property 不产生额外状态分配；
- 有状态 accessor 能正确构造、移动和销毁；
- compiled member accessor 热路径不发生明显退化；
- property、value、serialization 和跨模块测试全部通过；
- 单线程构建，避免当前环境并行编译导致 OOM；
- 每个实施阶段独立提交并推送。

## 非目标

- 不把整个 CAORS 改为 header-only；
- 不引入单独的 `property_vtable` / `method_vtable` 类型；
- 不采用 C++ 虚基类层次；
- 不把所有 meta 操作合并为一个巨型 request enum；
- 不在第一阶段重写 builtin ops、capability、any 或序列化格式；
- 不承诺插件 DLL 在仍有 wrapper 存活时可以卸载；
- 不以 wrapper 迁移为由删除现有类型模型一致性诊断。
