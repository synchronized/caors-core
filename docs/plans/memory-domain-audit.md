<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# 内存域调用点审计

## 审计状态

- 审计基线：2026-08-04，提交基线 `1f6e598`；
- 范围：`include/caors` 与 `src/caors` 中的生产 `.h` / `.cpp`；
- 目的：为 `memory-domain-redesign.md` 阶段 0、阶段 1 提供可复查的迁移清单；
- 本文只记录当前归属和目标归属，不改变现有分配行为。

## 术语与生命周期表

| 域 | Owner | 创建时机 | 释放时机 | 线程约束 | 插件约束 |
|---|---|---|---|---|---|
| process | host/runtime | runtime 初始化或首次稳定访问 | runtime 关闭或进程结束 | 内部同步 | 不保存可卸载插件对象或函数指针 |
| registry layer | primary/plugin layer | registry/layer 创建 | layer 安全 detach 后 | 注册受同步保护，冻结后只读 | 插件数据必须在 DLL 卸载前析构 |
| scratch | 同步调用或异步 request | 操作入口 | scope rewind 或 request 完成 | TLS scope 不跨线程 | 跨 DLL 由 host 显式传递 |
| result/user | 调用方、manager 或返回对象 | owner/API 入口 | owner/返回对象析构 | 继承 owner 契约 | 不能引用插件私有 allocator 或 TLS |
| infrastructure | resource/runtime 实现 | 静态初始化或资源构造 | 对应资源析构 | 由实现定义 | 仅作为其他域的后端，不独立表达业务生命周期 |

## 静态调用基线

使用以下符号扫描生产源码：

| 入口 | 调用/声明数量 | 当前语义 |
|---|---:|---|
| `library_resource()` | 54 | process、registry layer 与 infrastructure 混用 |
| `library_temporary_resource()` | 8 | 同步 TLS scratch |
| `get_default_memory_resource()` | 49 | result/user 默认值及部分子系统 owner 默认值 |
| `set_default_memory_resource()` | 2 | 默认资源 API 声明与实现 |

数量用于发现迁移期间的意外增长，不作为永久 API 数量目标。头文件中的 inline/template 调用按源码位置
计数，不按模板实例化次数计数。

## `library_resource()` 分类

### Process domain：保留但改用明确入口

| 区域 | 当前用途 | 目标 |
|---|---|---|
| `src/caors/core/string_name.cpp` | intern table 与稳定字符串存储 | process domain |
| `src/caors/meta/detail/type_identity.cpp` | type/capability identity 与诊断快照 | identity state 使用 process domain；返回快照改用 result domain |
| `src/caors/meta/module.cpp` | 默认 module 表 | 复审：全局 module identity 留在 process，可卸载 module record 进入 layer |

这些路径可以长期存在，但新代码应使用明确的 process owner/resource，而不是继续扩大
`library_resource()` 的含义。

### Registry layer domain：阶段 1 首批迁移

| 文件/区域 | 当前数据 | Owner | 释放时机 | 风险 |
|---|---|---|---|---|
| `meta/detail/registry_state.*` | registry state、snapshot layer 列表 | registry/layer | layer 销毁 | snapshot 可能延长 layer lease |
| `meta/registry_lifecycle.cpp` | primary/layer state 与 snapshot | registry/layer | registry/layer 销毁 | 当前固定全局资源 |
| `meta/registry_layers.cpp` | 发布后的 snapshot 列表 | registry snapshot | snapshot 最后引用释放 | 需避免插件卸载后残留 |
| `meta/detail/type_record.cpp` | `type_details`、metadata 与成员列表 | type record 所属 layer | record/layer 销毁 | 当前硬编码全局资源 |
| `meta/detail/property_record.h` | property metadata/accessor state | type record 所属 layer | record/layer 销毁 | callable 可能引用插件代码 |
| `meta/detail/method_record.h` | method metadata、参数名、默认参数、overload | type record 所属 layer | record/layer 销毁 | default value 有非平凡析构 |
| `meta/detail/enum_value_record.h` | enum metadata | type record 所属 layer | record/layer 销毁 | copy/move 必须保留资源 |
| `meta/metadata.h` | metadata map/value | owning record/layer | owner 销毁 | metadata value 可能持有动态对象 |
| `meta/enum_value.cpp`、`method.cpp`、`property.cpp`、`type.cpp` | 空 metadata fallback | process 或无分配 singleton | 进程结束 | 不应为每个 layer 重复分配 |
| `registration/batch.h`、`type_builder.h` | 注册批次与 pending 状态 | registration operation/layer | commit/rollback 完成 | 临时数据与提交数据需分离 |

阶段 1 要求：上述可卸载 record 对象图不再直接调用 `library_resource()`，由所属 layer resource 一路
传播到全部拥有型子对象。

### Scratch domain：阶段 3 迁移

| 区域 | 当前用途 | 目标生命周期 |
|---|---|---|
| `serialization/detail/temporary_memory.h` | typed serialization 临时 scope | 单次同步调用 |
| `serialization/messagepack.cpp` | parser/writer frame 与事务暂存 | 单次 encode/decode 或 writer operation |
| `meta/detail/method_record.*` | 参数补全、规范化、转换和 overload 准备（已接入） | 单次 invoke |
| `meta/detail/argument_access.h` | 内建参数转换结果（已接入） | 单次 invoke |
| JSON reader/writer 与 DOM reader context stack | 解析、写出栈与成员暂存（已接入） | 单次 parse/read/write |
| registry 查询局部 layer/snapshot vector | 查询调用 | 单次 query |

现有 8 处 `library_temporary_resource()` 主要集中在 serialization。阶段 3 不追求增加全局 TLS 访问点，
而是让核心实现接收显式 scratch scope，由同步 convenience API 在入口解析 TLS 默认值。

### Infrastructure：保留

以下调用属于 allocator/resource 实现自身，不迁移到 registry 或 result domain：

- `memory/resource.h` 与 `memory/resource.cpp` 的入口声明和实现；
- `memory/debug_tracking.cpp` 的内部记录 storage；
- `memory/size_class_pool.cpp` 的内部管理 storage；
- `memory/stack.cpp` 的 TLS scratch 实例。

这些资源仍需在后续复审中区分“资源实现自身状态”和“使用该资源分配的业务对象”。

## 默认资源调用分类

`get_default_memory_resource()` 的 49 处源码调用主要分为：

1. allocator、unique pointer、small vector 等基础类型在构造时捕获默认结果资源；
2. object、asset、filesystem、resource manager、task/executor 等长期 owner 在构造时解析资源；
3. IO、serialization 等返回值 API 在入口解析 result resource；
4. 少量模板/helper 在内部再次解析默认资源，需要检查是否已经错过 owner 传播机会。

目标规则：

- 长期 owner 只在构造入口解析一次并保存；
- 返回对象只在最外层 API 解析一次 result resource；
- 内部 helper 接收已解析的引用或非空指针；
- 析构和释放路径不重新读取默认资源；
- 多线程运行期不切换默认资源。

## 高风险边界

### 插件卸载

- layer record、metadata、default argument 与 callable 必须在 DLL 卸载前析构；
- process domain 只能保留不引用插件代码的 identity、GUID、稳定名称和诊断副本；
- snapshot 与 type handle 的 lease 可能延长 layer 生命周期，不能仅凭 registry map 移除就释放 storage。

### 异步操作

- 发起线程的 TLS scratch 不能进入 task/request；
- request 必须拥有独立 scratch/result owner，或显式借用生命周期覆盖 request 的上层 context；
- completion callback 执行线程不能假设与发起线程共享 TLS 实例。

### 跨域值

- `meta::any`、`variant`、string、vector 和默认参数可能拥有动态存储；
- scratch 到 result 必须 materialize；
- 不同 layer/result resource 之间的 move/swap 需要复制、拒绝或显式 unsafe 契约；
- arena rewind 前必须析构非平凡临时对象。

## 阶段 1 进入条件

- 本文生命周期表与调用分类成为迁移基线；
- 新增 `library_resource()` 调用必须在评审中说明所属域；
- `registry_options` 需要支持测试注入内部资源；
- tracking 测试必须分别统计 registry 外层 state 和完整 type record 对象图；
- 在修改 record 分配前先覆盖 layer 构造、注册、销毁和插件 detach 的无泄漏基线。

## 运行时基线

阶段 0 增加 `registry_options::internal_resource` 作为测试与后续 layer storage 注入入口，默认空值仍解析为
现有 `library_resource()`，不改变默认分配行为。

基线测试覆盖：

- 使用独立 `tracking_resource` 创建 registry；
- 注册包含 property 与 method 的类型；
- 确认 registry 外层状态产生可观测分配；
- registry 析构后 allocation/deallocation 数量相等、分配/释放字节相等且存活字节归零；
- 不固定编译器和 STL 相关的精确分配次数、字节数或峰值。

阶段 1 已让 `type_details`、property/method/enum metadata、wrapper 状态和 owned capability 继承
所属 layer resource，并验证独立 layer detach/销毁后 tracking resource 零存活分配。

阶段 2 新增 `process_resource()` 与顶层 `caors::runtime`，并迁移 `string_name`、type/capability
identity、全局 module 索引和静态空 metadata。源码与公共头中的 `library_resource()` 已从阶段 0 的
54 处收敛为仅剩兼容 API 的声明和定义；CAORS 内部不再调用该入口。CMake 配置阶段会扫描源码，发现
新增调用时直接报错，要求按生命周期选择 process、registry、scratch 或 result resource。
