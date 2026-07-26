<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Registry Overlay 与插件安全卸载开发计划

## 文档状态

- 当前状态：设计冻结，6A 至 6E 已完成，6F 实施中；
- 上游计划：`library-plugin-system.md` 阶段 6；
- 目标：插件注册可从新查询中原子移除，旧句柄和值不会调用已卸载 DLL 中的代码；
- 非目标：本阶段不实现热重载数据迁移，也不保证任意第三方线程可以被宿主强制终止。

## 当前实现约束

当前 `meta::registry` 是单一可变、append-only 状态：

- `registry_state::types` 独占拥有 `type_record`；
- name、GUID、C++ `type_info` 索引保存裸 `type_record*`；
- `meta::type` 保存 `registry* + type_record*`；
- `property`、`method`、`method_set` 等视图直接保存内部 record 指针；
- registry 写入和查询由同一 mutex 串行化，record 发布后地址稳定；
- `context` 只定位单个 registry，没有 layer 或 snapshot 语义。

这些假设使普通 append-only 注册简单且视图稳定，但不能通过“从 map 删除记录”实现插件卸载。
删除后旧视图会悬空；即使保留 record，只要其中 wrapper、cast、capability、codec 或 value ops 指向
插件代码，关闭 DLL 后仍会产生 use-after-unload。

## 必须覆盖的代码地址持有者

安全门禁不能只检查 `meta::type`：

- `type`、`property`、`method`、`method_set`、nested type 和 extension view；
- 编译后的 property/method accessor；
- capability、extension、parent cast 和 factory/constructor；
- `any`、`value`、`any_ref` 及容器 view 中保存的擦除 ops；
- 事件回调、任务 callable、completion callback 和服务函数表；
- 插件返回且需要插件析构函数的对象。

任何一项仍可达时，动态库 lease 必须保持有效。

## 方案选择

### 采用：不可变 Snapshot + 可分离 Layer + Library Lease

```text
registry
└─ atomic shared_ptr<const registry_snapshot>
   ├─ core layer
   ├─ application layer
   └─ plugin layer (generation N)
      ├─ immutable records and indexes
      ├─ active flag
      └─ shared library lease
```

- 写入先发生在未发布的 mutable layer draft；
- commit 完整校验 layer 内部及跨 layer 身份冲突；
- publish 生成新 snapshot，并原子替换 registry 当前 snapshot；
- 查询只读取一次 snapshot，不在查询过程中跨版本；
- detach 生成不包含目标 layer 的新 snapshot；
- layer 的 `active` 变为 false 后，旧公共句柄返回失效；
- 旧句柄和值仍持有 layer lease，保证其析构期间 DLL 不会提前关闭；
- 只有 layer 从当前 snapshot 移除且外部 lease 数归零，manager 才能真正关闭动态库。

### 不采用：直接删除 Record

无法保护旧裸视图、compiled accessor 和擦除值，风险不可接受。

### 不采用：只做 Tombstone

可以阻止新查询，但不能证明旧视图不会调用插件 ops；若永久保留 DLL 又失去安全卸载目标。

### 暂不采用：查询链上的可变 Overlay

多层 mutex、查询过程中 layer 移除和跨 layer parent/nested 关系会显著增加锁顺序及竞态。不可变
snapshot 更容易提供一次查询的一致视图。

## 身份与覆盖规则

第一版 overlay 不允许覆盖：

- 已存在的 `type_id`；
- 已存在的 canonical name；
- 已存在的 type GUID；
- 已存在的 C++ `type_info` 映射；
- 另一个 layer 已发布的同身份 extension。

插件只能新增记录或引用基础 layer 中已存在的父类型、字段类型和能力协议。这样 detach 后不会露出
被插件遮蔽的另一份不同行为，也不需要定义复杂覆盖优先级。

optional 服务不属于 meta registry，继续由 `plugin_host` 管理。

## 公共句柄与 Lease

新增内部 `registry_lease`，至少包含：

- layer generation；
- 原子 active 状态；
- 对 immutable layer 的共享所有权；
- 对插件 dynamic library control block 的共享所有权；
- 调试用 plugin ID 和来源路径。

以下对象必须持有或传递 lease：

- `type`；
- `property`；
- `method` / `method_set`；
- extension view 的迭代结果；
- compiled accessor；
- 从插件类型创建的拥有型 `any/value`。

`any_ref` 是非拥有视图。它本身不单独延长对象生命周期，但从带 lease 的 owner 创建时必须能让调用
路径验证 layer 仍 active。跨卸载边界长期保存裸 `any_ref` 仍属于调用方错误，正式文档需明确。

分离 layer 后：

- `valid()` 返回 false；
- 查询属性、方法、capability 和 extension 返回空；
- invoke/get/set/create 等操作失败，不进入插件代码；
- 已创建拥有型值仍保留 lease，并允许安全析构；
- 旧 generation 不会静默指向重新加载后的新 layer。

## Dynamic Library Lease

当前 `plugin_binary` 直接独占 `shared_library`，析构即关闭。需要拆为共享 control block：

```text
plugin_library_control
├─ platform::shared_library
├─ plugin_id + generation
├─ detached
└─ external lease count
```

- `plugin_binary`、published layer、公共句柄和值共享 control block；
- detach 只标记并移除查询可见性；
- `plugin_manager::unload()` 在 scope blocker 清空但仍有 lease 时返回 `pending/plugin_in_use`；
- 最后一个 lease 释放后才执行 `FreeLibrary/dlclose`；
- 动态库关闭不在 registry mutex 内发生。

第一版可使用 `std::shared_ptr` 完成正确性，再根据 profile 决定是否迁移到侵入式引用计数。

## 写入与发布流程

```text
create layer draft
  ↓
plugin initialize 使用绑定到 draft 的 registration context
  ↓
validate local records
  ↓
validate against current snapshot
  ↓
freeze immutable layer
  ↓
atomic publish new snapshot
```

initialize 失败时 draft 从未发布，可直接销毁并运行 scope 清理。publish 后不允许原地修改 layer；
后续追加注册必须创建替换 layer，并执行同样的校验和发布。

## 分离与卸载流程

```text
stop plugin
  ↓
shutdown plugin
  ↓
registration_scope.cleanup()
  ↓
检查 task/event/service/value blocker
  ↓
atomic detach registry layer
  ↓
active = false
  ↓
等待 snapshot reader / handle / value lease 归零
  ↓
close shared library
```

任何步骤失败都不得关闭 DLL。detach 之后若仍有 lease，插件状态为 `detached_pending_release`，而不是
`unloaded`。

## 并发模型

- registry 当前 snapshot 使用原子 `shared_ptr` 发布；
- 单次查询只捕获一个 snapshot；
- draft 写入仍由单写者或 draft mutex 保护；
- publish/detach 由 registry 写 mutex 串行化；
- 不在 registry 或 manager 锁内调用插件代码；
- detach 与旧查询并发时，查询要么看到旧 snapshot 并获得 lease，要么看到新 snapshot 并查不到；
- active 状态在调用插件 ops 前再次检查，避免 detach 后进入新调用；
- 已经进入插件函数的调用由 in-flight call guard 保活，detach 等待或报告 pending。

仅依赖 `shared_ptr` 不能阻止“检查 active 后、调用前”发生 detach，因此需要 layer 级
`in_flight_calls` guard。卸载门禁必须同时满足 inactive 且 in-flight 为零。

## 错误与诊断

需要补充插件错误语义：

- `plugin_in_use`：仍有句柄、值或 in-flight call；
- `layer_conflict`：overlay 身份与当前 snapshot 冲突；
- `stale_generation`：旧句柄或旧 manager handle；
- `detach_pending`：已从查询移除但尚不能关闭 DLL。

若暂不扩展通用 `errc`，第一批可以映射：

- `already_exists` / `inconsistent_type`；
- `pending`；
- `unsupported`。

manager 的结构化诊断必须同时给出 plugin ID、generation、lease 分类计数和当前生命周期阶段。

## 实施批次

### 6A：Snapshot 读取基础

- [x] 引入 `registry_snapshot` 所有权基础；
- [x] 现有单层 registry 先包装为 snapshot，不改变公开行为；
- [x] type、extension 和 capability 查询捕获 snapshot；
- [x] 保持现有 append-only registry/view/capability 测试通过；

并发 snapshot 发布测试需要 6B 的 layer publish 入口，随 6B 一起实现。当前 snapshot 内仍是受 mutex
保护的单层可变 state；不可变 layer 和原子替换从 6B 开始。

### 6B：Layer Draft 与发布

- [x] 增加 mutable layer draft；
- [x] registration builder 可通过 layer registry/context 写入 draft；
- [x] 实现跨 layer identity、C++ type_info、parent 和 nested 校验；
- [x] 发布后冻结 draft，并通过原子 snapshot 让新查询可见；
- [x] 覆盖基础 layer 查询回退、失败不污染和并发发布不丢失；

6B 不提供 detach；已发布 layer 在 6C 完成公共视图 lease 化前持续保留。第一版禁止覆盖当前
snapshot 中已有的 ID、名称、GUID 或 C++ type_info。draft 采用单写者模型，publish 与主 registry
写入通过主 state mutex 串行化。

### 6C：公共视图 Lease 化

- [x] `type/property/method/method_set/extension_view/accessor` 传递 lease；
- [x] layer inactive 后视图确定失效；
- [x] 旧 generation 不指向新记录；
- [x] 覆盖句柄复制移动约束和 detach 后失效行为。

`extension_view` 保存创建时的 snapshot lease，但扩展内容仍按 `type_id` 从当前 registry 复制查询；
detach 后旧 view 可保留 ID 快照，但不再返回已分离 layer 的扩展。`find_extension_ptr()` 仅允许访问
registry 自己的 primary layer，跨 overlay 查询统一使用 `find_extension()`，并在 snapshot 与 state
锁保护期间完成副本构造。

### 6D：擦除值与调用 Guard

- [x] `any/value` 通过其 `meta::type` 保存产生 ops 的 layer lease；
- [x] capability、property、method、factory、codec 和 builtin view 调用增加 in-flight guard；
- [x] inactive layer 拒绝新调用；
- [x] 验证旧值可在 detach 后安全析构；
- [x] set/associative iterator 自持 type lease 和 ops，不依赖外部 view 生命周期。

`layer_call_guard` 在调用开始时先检查 active，再增加 in-flight 计数并二次确认 active；detach
与新调用竞争时，新调用要么完整进入并计数，要么确定失败。析构路径不要求 layer active，
拥有型值和容器 iterator 使用已经缓存的 destroy 回调清理资源；后续 6E 由共享 library control
block 保证这些回调执行前动态库不会关闭。

### 6E：Plugin Manager 接入

- [x] `plugin_binary` 改用 library control block；
- [x] library lease 可在发布前绑定到 registry layer；
- [x] manager 为每个插件创建 registration layer 和 scope；
- [x] initialize 失败销毁未发布 draft；
- [x] stop/shutdown 后 detach；
- [x] lease 未归零返回 pending，不关闭 DLL；
- [x] 增加版本化 meta registration host extension，让插件 initialize 写入对应 draft。

`plugin_binary` 与 `plugin_library_lease` 现在共享动态库 control block；control block 绑定到
`registry_layer_state`，因此 layer owner 被销毁后，旧 type/property/method/owning value 持有的
layer lease 仍会保持 DLL 已加载。绑定只允许发生在 publish 前，避免已发布 layer 的动态库所有权
在并发查询期间发生变化。

`plugin_manager` 已提供稳定只读 handle、重复 ID 拒绝、串行生命周期、自动
stop/shutdown/cleanup/detach，以及重新加载 generation 隔离。当前 ABI 尚未提供 meta
registration host extension。扩展只在 C ABI 中传递 `owner + opaque void*`，公共 C++ 适配层负责
还原 `meta::registry*`；绑定只在 initialize 调用期间有效，返回后立即移除，插件不能保存并继续
修改已经发布的 layer。

### 6F：真实跨 DLL 卸载测试

- [x] 插件注册 type/property/method/capability/codec；
- [ ] host 保存并释放各类旧视图；（type/property/method/accessor/factory/constructor/
      extension/container/composite/indirect/codec 已覆盖）
- [x] 验证 detach 后旧视图失效；
- [x] 验证旧 owning value 销毁前 DLL 不关闭；
- [x] 验证最后 lease 释放后 DLL 关闭；
- [x] 覆盖重复加载、新 generation、并发查询和 in-flight method；
- Windows 与 POSIX，Debug/Release，ASan 可用时运行 use-after-unload 检查。

真实 failure plugin 会先通过 host extension 向 draft 注册类型，再让 initialize 返回失败；测试确认
draft 被销毁且主 registry 不可见该类型。并发查询测试持续解析插件类型，同时执行 detach/unload，
确认查询只会观察到有效旧 snapshot 或已移除的新 snapshot，并连续运行 10 次检测竞态稳定性。
协议插件还注册了 factory 与 `std::vector` property：host 验证 accessor、factory 和 owning value
在 detach 后按 registry generation 失效；已经绑定到成员值且 ops 位于宿主模块的 sequence view
仍可安全读取，并在显式释放前参与其自身依赖的生命周期管理。
协议插件进一步注册可复制 meta extension、`std::unordered_set` 与 `std::unordered_map`
property：host 验证 extension view 在 detach 后失效且不再返回扩展副本；已经绑定到成员值的
set/associative view 仍可读取，随后在 owning value 析构前显式释放。
pair/tuple/variant/indirect view 也通过插件对象成员在宿主绑定；detach 后这些使用宿主静态 ops 的
已绑定视图仍可安全读取，并在 owning value 析构前显式释放。
带一个 `int` 参数的 constructor 也由插件注册并在宿主调用；测试校验构造参数、detach 后 facade
失效，以及 constructor owning value 在动态库仍被 lease 保活时安全析构。

## 每批验收门禁

- 所有现有 meta/serialization/object 测试保持通过；
- 不在公共 API 暴露 `type_record`、snapshot 或平台句柄；
- 不在锁内调用插件代码或关闭动态库；
- 不允许旧句柄静默访问新 generation；
- 任何无法证明安全的状态都返回 pending/unsupported；
- Windows Clang Shared Debug 使用 `--parallel 1`；
- 每个批次独立提交并推送。

## 主要成本与风险

- 公共 meta 句柄会增加一个 lease 字段，复制成本和对象大小上升；
- `shared_ptr` 原子引用计数会增加高频反射查询成本；
- 所有产生衍生视图的路径都必须正确传播 lease，漏一处就可能形成安全漏洞；
- 擦除值和非拥有 `any_ref` 的边界需要额外测试与正式文档；
- property/method wrapper 当前散布在多种 record 中，迁移量较大；
- snapshot 发布会复制索引或 layer 列表，需要在正确性完成后再 profile；
- 插件线程和第三方原生指针无法由 registry 方案自动治理，仍依赖插件生命周期协议。

## 冻结决策

- 采用不可变 snapshot，不采用可变查询链；
- 第一版 overlay 禁止覆盖已有身份；
- detach 后旧视图失效，但 lease 继续保活 DLL；
- owning erased value 必须持有 lease；
- 使用 in-flight call guard 封闭 active-check 竞态；
- 正确性阶段先使用 `std::shared_ptr`，性能优化后置；
- 阶段 6 按 6A 至 6F 独立提交，不做单次大爆炸迁移。
