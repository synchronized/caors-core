<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Registry 查询快路径与 Layer Guard 收敛计划

## 文档状态

- 当前状态：阶段 1 至阶段 7 及 Windows、Linux 跨平台复验已完成；
- 上游背景：`registry-overlay-safe-unload.md` 已引入 registry snapshot、插件 layer 和 library lease；
- 目标：在保持插件安全卸载边界的前提下，把普通 meta 查询收敛为低成本只读路径；
- 非目标：不改变插件 C ABI，不引入跨 runtime 共享 `meta::type`，不让插件卸载绕过 layer lease。

## 问题

当前 registry 查询路径偏保守：

- `registry_state` 带 `mutex`；
- `find_type()`、capability/schema 查询和 snapshot layer 遍历会在读路径加锁；
- 多数 view/facade 调用会进入 `layer_call_guard`；
- 主程序类型和插件类型在调用成本上区分不明显。

这保证了早期并发安全，但与长期目标不完全一致。注册类型发布后应不可变，查询应接近 RTTR/EnTT
一类库的只读快路径：注册或 publish/detach 阶段同步，普通查询不加锁。

## 目标结构

```text
registry
└─ atomic shared_ptr<registry_snapshot>
   ├─ pinned/process layer     查询无锁，call guard no-op
   └─ detachable/plugin layer  查询无锁，调用插件 ops 时 in-flight guard
```

期望成本：

- registry 查询：一次 atomic snapshot load，加只读 map/vector 查询；
- 主程序类型调用：不做 in-flight 原子计数；
- 插件类型调用：进入插件函数指针前做 active + in-flight guard；
- 纯 metadata 查询：不进入插件代码，不使用调用级 guard。

## 设计原则

### Published layer 不可变

未发布 draft layer 可以继续使用 mutex 保护写入。`publish_layer()` 成功后：

- `registry_state::frozen == true`；
- `type_record`、property、method、nested type、enum value、metadata 和 capability 不再修改；
- name/GUID/C++ type 索引不再修改；
- 查询可以不再锁该 layer。

主 registry 如果仍支持初始化期注册，可以在运行期前显式 freeze，或按“写入路径加锁、已冻结后读无锁”
分阶段实现。

### Snapshot 是查询一致性边界

查询入口先读取当前 snapshot：

```cpp
auto snapshot = registry.capture_snapshot();
```

之后只遍历 snapshot 中的 layer。publish/detach 通过原子替换 snapshot 改变新查询可见性，旧查询继续使用
旧 snapshot，不观察半发布状态。

### Lease 与 Call Guard 拆分

`meta::type`、property、method 和运行期 view 保存 layer lease，用来保证 layer state 和相关
动态库生命周期不悬空。

`layer_call_guard` 只表达“正在进入可能属于可卸载模块的函数指针”。它不应被普通 metadata 查询滥用。

### Layer 生命周期策略

给 layer 增加生命周期分类：

```cpp
enum class registry_layer_lifetime {
  pinned_process,
  detachable,
};
```

这里推荐使用 `pinned_process`，不使用 `static` 作为公开枚举名。原因是 `static` 容易和静态链接、
静态初始化、静态存储期混淆；实际语义是“该 layer 被钉在进程生命周期内，不支持提前卸载”。

`pinned_process` layer 不支持 detach/unload，`layer_call_guard` 对它直接成功，不做原子 inc/dec。
`detachable` layer 在调用插件 ops 前执行：

```text
check active
increment in_flight
recheck active
call
decrement in_flight
```

插件加载 API 可以暴露同一套语义：

```cpp
enum class plugin_lifetime {
  pinned_process,
  detachable,
};

struct plugin_load_options {
  plugin_lifetime lifetime = plugin_lifetime::detachable;
};
```

`detachable` 是当前默认策略，保留显式热卸载语义。`pinned_process` 插件加载后跟随
`plugin_manager` 或进程生命周期，`plugin_manager::unload()` 应返回 `errc::unsupported`
或明确的状态错误，不执行 `detach_layer()` / `FreeLibrary`。它的 registry layer
可以走 `pinned_process` fast path，不承担插件热卸载的 in-flight 原子成本。

`detachable` 插件显式选择热卸载语义，发布后的 layer 可 detach，进入插件 ops 前必须使用 call guard，
unload 前仍要检查 layer lease、in-flight 调用、任务、服务和插件 owned erased value。

### Registry 管理策略命名

registry 自身的管理策略和插件 / layer 生命周期是两件事，应使用不同命名：

- `registry_management_policy`：描述 registry 如何处理注册、冻结和查询同步；
- `registry_layer_lifetime`：描述单个 layer 是否可提前 detach；
- `plugin_lifetime`：描述插件动态库是否可提前 unload。

不推荐把 registry 策略命名为 `registry_lifetime` 或 `plugin_strategy`。前者容易和 layer / 插件生命周期混淆，
后者会把 registry 的线程同步策略和插件卸载策略绑在一起。

第一版 registry 管理策略建议只暴露三档：

```cpp
enum class registry_management_policy {
  locked_append_only,
  frozen_read_optimized,
  manual_synchronization,
};

struct registry_options {
  registry_management_policy management = registry_management_policy::locked_append_only;
};
```

语义边界：

- `locked_append_only`：默认策略。注册、publish、detach 和查询都由 registry 内部同步；类型一经 commit
  不支持隐式 replace，适合作为早期保守默认。
- `frozen_read_optimized`：初始化期允许注册，显式 `freeze()` 或 `init()` 后进入只读快路径；冻结后 primary
  写入返回 `errc::frozen`。已发布 layer 仍按 snapshot 参与查询，detachable layer 可继续 detach。
- `manual_synchronization`：registry 不为普通查询提供内部互斥保护，调用方必须保证注册 / publish / detach
  与查询调用没有数据竞争。当前已经保留策略入口，内部仍按保守同步路径执行；真正跳过查询锁需要单独实现。

策略命名刻意使用行为描述，而不是实现结构名：

- 不叫 `append_only_registry`，因为未来可以保持 append-only 语义但换内部实现；
- 不叫 `snapshot_registry`，因为 snapshot 是所有策略都可能使用的查询一致性机制；
- 不叫 `mutable_registry`，因为当前长期方向仍是不支持随意 replace 已发布类型；
- 不叫 `static`，避免和 C++ 静态存储期、静态链接和 `pinned_process` 插件混淆。

## 需要整理的路径

### Registry 查询

- `find_type(type_id/name/guid/type_info)`；
- `contains()`；
- `is_a()` / `upcast()`；
- serialization schema 查询。

这些路径应只依赖 snapshot 和不可变 layer，不应在 frozen layer 上加 mutex。

### Type 和 Metadata 查询

- `type::id/name/guid/kind/size/alignment`；
- `type::property_count/method_count/nested_count`；
- `type::metadata/find_metadata`；
- `property`/`method` 的名称、参数类型、返回类型等纯记录查询。

这些路径不进入插件函数指针，原则上只需要 layer active 检查或记录 lease，不需要 in-flight guard。

### Ops 调用

以下路径仍需要 detachable layer 的 call guard：

- `method::invoke*()`；
- `property::get/set/get_ref/get_any`；
- `factory_capability` / `constructor_capability`；
- `any` 复制、析构、赋值和默认构造；
- sequence/set/associative/pair/tuple/variant/indirect view 访问或修改实例；
- object ptr/handle view 操作；
- serialization codec read/write/make_value；
- 用户 capability facade 绑定后调用内部 ops。

## 分阶段实施

### 阶段 1：补充 layer lifetime 标记

- [x] 在 `registry_layer_state` 增加 `registry_layer_lifetime`；
- [x] primary/default layer 标记为 `pinned_process`；
- [x] `registry::create_layer()` 默认创建 `detachable` layer，保持现有插件 detach 行为；
- [x] `registry::create_layer(registry_layer_lifetime)` 支持显式创建 pinned 或 detachable layer；
- [x] `layer_call_guard` 对 `pinned_process` 做 no-op；
- [x] 保持现有测试通过。

完成标准：主 registry 类型和 pinned 插件类型调用不再产生 in-flight 计数变化，detachable 插件 layer 行为不变。

### 阶段 2：补充 registry 管理策略 API

- [x] 增加 `registry_options` 和 `registry_management_policy`；
- [x] `registry()` 默认使用 `locked_append_only`，保持现有行为；
- [x] 文档说明 `locked_append_only`、`frozen_read_optimized` 和 `manual_synchronization` 的线程安全责任；
- [x] 暂时不增加多个 registry 派生类，避免策略和存储结构过早绑定。

完成标准：用户可以在创建 registry 时选择策略，但默认行为不变。

### 阶段 3：插件加载 lifetime 选项

- [x] 增加 `plugin_load_options` 和 `plugin_lifetime_policy`；
- [x] `plugin_manager::load(path)` 保留便捷入口，默认使用 `detachable`；
- [x] `plugin_manager::load(path, options)` 允许显式选择 `pinned_process`；
- [x] `plugin_manager::unload()` 遇到 `pinned_process` 插件返回不支持，不 detach layer；
- [x] 文档说明 pinned 插件是无法提前卸载的低开销模式，detachable 是可热卸载模式。

完成标准：pinned 插件无法提前 unload，detachable 插件保留现有 unload/detach 行为。

### 阶段 4：区分 metadata 查询和 ops 调用

- [x] 盘点所有 `layer_call_guard` 使用点；
- [x] `method_set::match/resolve` 只做已注册签名匹配，不进入 call guard；
- [x] 纯 metadata / capability 查询只做 snapshot 记录读取；
- [x] 真正调用函数指针的路径继续使用 call guard；
- [x] 继续拆分 `type::get_value_type()` / `get_key_type()` 这类依赖 ops 回调的类型查询，
  长期应把常见内置类型信息固化到记录或安全 facade；
- [x] 为 detach 后纯查询和 detach 后调用分别补测试。

完成标准：detach 后旧 view 查询基础 id/name 可按文档定义处理，调用插件 ops 明确失败。
当前 `type_record::semantics()` 已缓存常见 pointer/wrapper/sequence/set/associative/pair
的 key/value 类型关系，`type::get_value_type()` / `get_key_type()` 不再通过容器 ops 进入
detachable layer 调用 guard。

### 阶段 5：冻结 layer 查询无锁

- [x] 明确 `registry_state::frozen` 后不可变；
- [x] 为 frozen layer 增加无锁只读查找 helper；
- [x] `find_type()`、`contains()`、`size()` 和 capability 查询改为 snapshot + frozen fast path；
- [x] 未冻结主 layer 仍保留锁，已发布插件 layer 查询不再加 `state.mutex`。

完成标准：已发布 layer 查询不再加 `state.mutex`；并发 publish/detach/query 测试通过。

### 阶段 6：主 registry freeze 策略

- [x] `registry_management_policy::frozen_read_optimized` 在 `init()` 成功后冻结 primary layer；
- [x] 默认 `locked_append_only` 仍允许后续注册并保留查询锁；
- [x] 文档说明运行期查询推荐使用 frozen registry 或已发布 layer；
- [x] 增加显式 `registry::freeze()`，定义它与延迟注册和插件 publish 的交互。

完成标准：普通应用初始化完成后，默认 registry 可进入无锁查询模式。

### 阶段 7：性能与回归验证

- [x] 增加 registry find/property/method/schema 查询 micro benchmark；
- [x] 对比加锁查询与 frozen fast path 的 benchmark 用例；
- [x] 使用 `windows-clang-debug` 和 `windows-clang-shared-debug` 覆盖普通 registry 与真实插件拓扑；
- [x] 使用 Linux Clang/GCC Static、Clang Shared 与 ASan 覆盖 registry 和插件卸载回归；
- [x] 文档更新 `module-boundary.md`、`dll-plugins.md` 和 `meta.md`。

当前已覆盖 `locked_find_type_id`、`frozen_find_type_id`、`layer_find_type_id`、
`frozen_contains`、`frozen_property_lookup`、`frozen_method_lookup`、
`locked_schema_lookup`、`frozen_schema_lookup` 和 `layer_schema_lookup`。
Linux 基础矩阵与独立 ASan 插件压力验证结果记录在
[linux-validation.md](../checklists/linux-validation.md)。

完成标准：查询快路径有可测收益，插件 detach/unload 安全测试继续通过。

## Benchmark 摘要

`windows-clang-benchmark`、提交 `7015a59`、`100000` 次迭代的快速样本显示：

| case | time acc | per iter |
| --- | ---: | ---: |
| `meta.registry.locked_find_type_id` | 4.71 ms | 47.10 ns |
| `meta.registry.frozen_find_type_id` | 3.70 ms | 37.00 ns |
| `meta.registry.layer_find_type_id` | 3.70 ms | 37.00 ns |
| `meta.registry.locked_schema_lookup` | 6.08 ms | 60.80 ns |
| `meta.registry.frozen_schema_lookup` | 4.65 ms | 46.50 ns |
| `meta.registry.layer_schema_lookup` | 4.62 ms | 46.20 ns |
| `meta.registry.frozen_contains` | 2.83 ms | 28.30 ns |
| `meta.registry.frozen_property_lookup` | 9.22 ms | 92.20 ns |
| `meta.registry.frozen_method_lookup` | 1.14 ms | 11.40 ns |

该结果只作为同机同 preset 的趋势记录。当前数据说明 frozen primary 和已发布 layer 的只读查询成本低于
默认 locked primary；schema 查询也能走同一套 snapshot/frozen fast path。完整摘要见
`benchmarks/results/2026-07-27-windows-clang-benchmark-7015a59.md`。

## 风险

- 如果某条写入路径绕过 builder 修改已发布 `type_record`，无锁查询会产生数据竞争；
- capability data 如果内部保存可变对象，即使 capability 容器不可变，也需要调用方自行同步；
- pinned layer fast path 不能误用于可卸载插件 layer；
- pinned 插件虽然不能 unload，但仍可能被 stop；插件服务 API 应自行定义 stop 后是否允许继续调用；
- 旧 view 的“detach 后哪些纯查询仍可用”需要文档和测试统一，不应出现有的返回快照、有的返回空的混乱行为。
