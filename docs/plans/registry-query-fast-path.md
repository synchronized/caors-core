<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Registry 查询快路径与 Layer Guard 收敛计划

## 文档状态

- 当前状态：设计草案；
- 上游背景：`registry-overlay-safe-unload.md` 已引入 registry snapshot、插件 layer 和 library lease；
- 目标：在保持插件安全卸载边界的前提下，把普通 meta 查询收敛为低成本只读路径；
- 非目标：不改变插件 C ABI，不引入跨 runtime 共享 `meta::type`，不让插件卸载绕过 layer lease。

## 问题

当前 registry 查询路径偏保守：

- `registry_state` 带 `mutex`；
- `find_type()`、extension 查询和 snapshot layer 遍历会在读路径加锁；
- 多数 view/facade 调用会进入 `layer_call_guard`；
- 主程序类型和插件类型在调用成本上区分不明显。

这保证了早期并发安全，但与长期目标不完全一致。注册类型发布后应不可变，查询应接近 RTTR/EnTT
一类库的只读快路径：注册或 publish/detach 阶段同步，普通查询不加锁。

## 目标结构

```text
registry
└─ atomic shared_ptr<registry_snapshot>
   ├─ static/process layer     查询无锁，call guard no-op
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
- `type_record`、property、method、nested type、enum value、capability 和 extension 不再修改；
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

`meta::type`、property、method、view 和 extension_view 保存 layer lease，用来保证 layer state 和相关
动态库生命周期不悬空。

`layer_call_guard` 只表达“正在进入可能属于可卸载模块的函数指针”。它不应被普通 metadata 查询滥用。

### Static Layer Fast Path

给 layer 增加生命周期分类：

```cpp
enum class registry_layer_lifetime {
  static_process,
  detachable,
};
```

`static_process` layer 不支持 detach/unload，`layer_call_guard` 对它直接成功，不做原子 inc/dec。
`detachable` layer 在调用插件 ops 前执行：

```text
check active
increment in_flight
recheck active
call
decrement in_flight
```

## 需要整理的路径

### Registry 查询

- `find_type(type_id/name/guid/type_info)`；
- `contains()`；
- `is_a()` / `upcast()`；
- `has_extension()`；
- `find_extension()`；
- `extension_view()`；
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

- 在 `registry_layer_state` 增加 `registry_layer_lifetime`；
- primary/default layer 标记为 `static_process`；
- plugin/draft layer 标记为 `detachable`；
- `layer_call_guard` 对 `static_process` 做 no-op；
- 保持现有测试通过。

完成标准：主 registry 类型调用不再产生 in-flight 计数变化，插件 layer 行为不变。

### 阶段 2：区分 metadata 查询和 ops 调用

- 盘点所有 `layer_call_guard` 使用点；
- 纯记录查询只做有效性检查，不进入 call guard；
- 真正调用函数指针的路径继续使用 call guard；
- 为 detach 后纯查询和 detach 后调用分别补测试。

完成标准：detach 后旧 view 查询基础 id/name 可按文档定义处理，调用插件 ops 明确失败。

### 阶段 3：冻结 layer 查询无锁

- 明确 `registry_state::frozen` 后不可变；
- 为 frozen layer 增加无锁只读查找 helper；
- `find_type()`、`has_extension()`、`find_extension()`、`extension_view()` 改为 snapshot + frozen fast path；
- 未冻结 draft layer 仍只允许注册写入，不进入主查询 snapshot。

完成标准：已发布 layer 查询不再加 `state.mutex`；并发 publish/detach/query 测试通过。

### 阶段 4：主 registry freeze 策略

- 增加显式 `registry::freeze()` 或在 `init()` 后冻结默认 registry；
- 文档说明默认运行期查询推荐在 freeze 后进行；
- 如果保留运行期主 registry 注册，定义它与无锁查询的同步边界。

完成标准：普通应用初始化完成后，默认 registry 可进入无锁查询模式。

### 阶段 5：性能与回归验证

- 增加 registry find/property/method/schema 查询 micro benchmark；
- 对比加锁查询与 frozen fast path；
- 使用 `windows-clang-debug` 和 `windows-clang-shared-debug` 覆盖普通 registry 与真实插件拓扑；
- 文档更新 `module-boundary.md`、`dll-plugins.md` 和 `meta.md`。

完成标准：查询快路径有可测收益，插件 detach/unload 安全测试继续通过。

## 风险

- 如果某条写入路径绕过 builder 修改已发布 `type_record`，无锁查询会产生数据竞争；
- extension value 如果内部保存可变对象，即使 extension 容器不可变，也需要调用方自行同步；
- 静态 layer fast path 不能误用于可卸载插件 layer；
- 旧 view 的“detach 后哪些纯查询仍可用”需要文档和测试统一，不应出现有的返回快照、有的返回空的混乱行为。

