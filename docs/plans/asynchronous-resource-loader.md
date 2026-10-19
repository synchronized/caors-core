<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# 异步资源 loader 开发计划

## 状态与范围

状态：已完成开发验收，纳入 [v0.3.0](../versions/v0.3.0.md)；实现、专项回归与常规 CI 已通过，公开状态见版本页。
目标为可延迟交付结果的加载协议，接入现有 resources/database 与 assets/asset_manager。
版本范围和发布门槛由版本页维护；当前同步接口见[资源参考](../reference/resources.md)与[Assets 参考](../reference/assets.md)。

改动前，`loader<T>::load()` 必须同步返回 `load_result<T>`，`request_load()` 只是把该调用放入后台 executor。
[外部 executor](../reference/tasks.md#外部-executor)解决执行位置，不能让同步 loader 在返回后交付 I/O 结果。
新协议要释放发起任务所在的 worker，而不是在 worker 中等待宿主回调。

## 已确定的设计方向

### 公共接口与注册

新增独立异步 loader 协议，保留原同步 `loader<T>`，不要求所有已有实现重写。
采用 `async_loader<T>`、`async_load_context` 与 move-only `load_completion<T>`；
精确当前协议由[资源参考](../reference/resources.md#原生异步-loader)及公共头维护。

同一类型允许注册同步与异步能力。异步请求优先异步能力，只有同步 loader 时通过现有后台 executor 适配。
启动被拒绝或运行失败不自动回退重试另一 loader，避免重复 I/O 和掩盖错误。
同步入口缓存命中时沿用原查询；缓存未命中且仅有异步 loader 时返回 `errc::unsupported`，
不阻塞等待、不嵌套 pump 宿主 loop。类型绑定与注册冲突检查沿用现有规则，并在实施时补覆盖。

### asset_manager 默认调度与统一请求入口

在构造或运行期初始化 asset_manager 时传入默认 `worker` 与 `completion` executor，
默认值属于运行期调度配置，不写入可序列化的 `project_config`。保留原构造方式，兼容仅使用同步入口的调用方。
配置类型为 asset_execution_options，构造/初始化和 group 用法以[Assets 参考](../reference/assets.md#默认调度与-asset_group)为准。以下保留设计示例：

```cpp
// 通过构造或 set_execution_options 配置默认调度组合。
auto request = assets.request_load<texture>(guid);

// 特殊调度提前绑定到 group，后续请求不再重复传 executor。
auto group = assets.create_group(other_worker, other_completion);
auto special = group.request_load<texture>(guid);
```

统一请求入口通过同一条路由处理缓存命中、同步 loader 后台适配与异步 loader 延迟完成。
缓存命中可能立即完成；请求入口不保证一定切换线程或挂起协程。保留同步加载接口，不要求业务混用两套入口。
默认调度用于 request_load、request_reload 以及同类型/跨类型 request_preload；特殊调度通过下节 asset_group 复用。
现有显式传入完整 executor 组合的重载保留兼容，作为底层入口，不要求业务逐次传参。
一次请求选用 manager 默认组合或 group 固定组合，不隐式拼接部分默认句柄。

`worker` 用于同步 loader 后台适配、异步发起调度及需要的解析/计算任务；宿主 I/O loop 的线程转发仍由适配器负责。
`completion` 必须对应 cache 所属线程。按次覆盖 completion 只改变调度路径，不迁移 cache 的线程归属。

调度配置与寿命约定如下：

- manager 保存可复制的 executor 句柄，不拥有其线程池、execution_context 或宿主 I/O loop，
  也不因设置默认值而自动创建执行线程。句柄仍可访问不等于后端仍可执行任务。
- 未配置默认调度组合时，无显式 executor 的异步入口返回失败请求，错误为 `errc::invalid_argument`；
  不回退到 inline_executor，不隐式创建线程池。原显式传参入口和同步入口仍可使用。
- 请求保存发起时选定的 executor 句柄快照。向已关闭环境投递时，沿用现有 frozen/投递失败终态与清理规则，
  不改投另一个调度器。backend 发起、解析、完成任务被拒绝或丢弃时均须收口。
- 在途请求存在时不允许更换默认调度。请求终态不等于底层 I/O 已结束：重配前需停止新请求、清理实际 I/O、
  回调和队列工作，并在所属线程进行。可检测的不安全重配应返回明确错误；不提供任意线程并发 setter。
- 无在途操作的重配不能迁移已有 cache 的所属线程；需要更换完成线程时，按清空/重建 cache 与 manager 的寿命协议处理。
- 宿主负责让执行上下文与长期内存资源覆盖实际操作寿命；销毁、clear 或重新 apply 配置前，先执行前述清理协议。
  完成队列需要主动 pump 时，不能阻塞其所属线程等待必须由该队列提交的请求。

### asset_group：复用调度组合的轻量入口

采用 `assets::asset_group`，由 `asset_manager::create_group(worker, completion)` 创建。
当前接口的精确契约见 Assets 参考；设计用法如下：

```cpp
auto group = assets.create_group(other_worker, other_completion);
auto request = group.request_load<texture>(guid);
auto reload = group.request_reload<texture>(guid);
auto preload = group.request_preload(root);
```

manager 使用初始化时的默认调度；group 保存创建时明确传入的完整调度组合，后续调用省略 executor。
group 不依赖 manager 是否配置默认调度，也不随默认值重配而隐式更换句柄；不提供组内修改调度器的 setter。
原显式传参入口与 group 转发到同一条内部请求路径，不实现平行的缓存或请求系统。

| group 入口 | 操作语义与返回类型 |
| --- | --- |
| `request_load<T>(guid)` | 获取单个资源，命中 cache 立即返回，否则发起或订阅加载；返回现有 `load_request<T>` |
| `request_reload<T>(guid)` | 重新加载单个资源，成功替换 cache，失败保留旧绑定；返回现有 `load_request<T>` |
| `request_preload(root)` | 按 manifest/已登记依赖图加载根资源及跨类型依赖闭包，复用缓存而不强制刷新；返回现有 `preload_request` |
| `request_preload<T>(root)` | 同类型依赖闭包预加载，返回现有 `preload_request` |

group 只关联 manager、runtime 配置代次和 executor 句柄，不拥有 manager、线程池、I/O loop 或独立 cache。
可复制/移动的 group 副本复用同一 manager 与句柄，不复制资源，也不因复制或析构改变已发请求的订阅状态。
manager 与依赖服务必须覆盖 group 全部访问及实际操作的寿命；不承诺 manager 销毁后 group 仍可访问。
请求保存所需调度快照，因此 group 先析构不会自动取消请求；宿主仍须清理实际 I/O 和回调。

manager clear/重新 apply 配置使旧 group 失效：在 manager 仍存活时，旧 group 请求返回失败状态，不静默绑定新配置；
重新配置后显式重建 group。采用 database 配置代次检查，旧 group 返回 frozen。单纯更换默认调度不等于更换 runtime 配置，
不能据此改变现有 group 的固定句柄。无效/已关闭的 group 调度组合不回退到 manager 默认值。
completion 仍须运行在同一 cache 所属线程，创建 group 不迁移资源线程归属。

#### 跨 group 的共享加载

同一 manager 下，命中同一 cache/GUID 且符合现有操作种类共享规则的请求继续复用底层工作；
group 不作为隔离键，不能仅因不同 group 重复 I/O。load 与 reload 不因 GUID 相同而被无条件合并。
首次创建底层操作的请求确定其 worker 与 completion；后续订阅不改变实际调度器。
新订阅提供的 group 因而不是底层任务的强制执行位置，也不保证其等待方在该 group 的 completion 恢复。
需要指定 coroutine 恢复线程时显式使用现有调度适配，不在本版增加每订阅完成队列。

不同 group 的订阅仍独立取消；既有 `cancel_shared()` 继续影响共享操作，不能被解释为仅取消本 group。
第一版不加入 group 级统一取消、批量等待、独立缓存、资源驻留或自动卸载职责；preload 的批次语义由原请求维护。

### 启动、完成与所有权

启动接口立即返回接受/拒绝状态；同步拒绝时不保留完成凭证，拒绝后不得执行或回调。
接受后可以在启动返回前同步完成，也可以从任意获准线程稍后完成；必须通过同一凭证收口一次。
完成入口接管成功资源或错误，防止重复终态；迟到/重复结果不能泄漏或二次提交 cache。
完成凭证被销毁而未交付时，将仍待处理的操作收口为失败，不允许永久遗留 pending。
完成回调可重入，库内部锁不覆盖宿主代码、用户回调或资源析构。

加载操作状态复用现有 `load_request<T>`，不另建第二套公共 request/cancellation 系统。
后台执行结束不等于 I/O 结束；完成凭证、结果内存资源、进度通道与模块 pin 必须覆盖实际操作寿命。
公共 request 达到终态也不等于可以立刻销毁仍有迟到回调的 backend。

### 读取、解析与提交线程

```text
request_load / request_reload / request_preload
  → 异步 loader 发起宿主读取并立即返回
  → I/O 完成后按需投递计算 executor 解析、构造资源
  → completion executor 在 cache 所属线程提交
  → 完成现有 load_request，通知订阅与 coroutine
```

宿主决定如何向 loop 线程投递启动/取消工作；CAORS 不假设 backend API 可从任意线程调用。
I/O 完成后是否需要计算 executor 由 loader 决定，避免耗时解析阻塞宿主 loop。
最终 cache commit、generation 与 reload 通知沿用现有完成线程约束，完成任务被拒绝或丢弃时也必须收口。
临时缓冲区不得跨异步边界借用 TLS scratch；未提交结果的销毁线程、allocator 并发要求需写入公共注释。

### 取消与关闭

单订阅 `cancel()` 只影响该订阅。最后订阅放弃或 `cancel_shared()` 时，请求底层操作协作取消；
取消请求允许无法中断或已在执行的 I/O，不能把取消成功当成 I/O 已结束。
保留现有 cache 提交线性化边界：提交开始前阻止已放弃操作入 cache，提交开始后不承诺回滚。
取消后的迟到结果只做清理，不恢复取消的订阅、不二次通知，不绕过既有缓存提交规则。

后端 operation、请求结构、缓冲区、loader/provider/source、长期内存资源与实现模块保持到实际回调/任务清理完成。
manager 的析构、重新 apply 配置与 clear 前先停止新请求并 drain/cancel 实际在途工作；
本版明确主机清理协议，不新增通用自动卸载或任意并发销毁 manager 的保证。
底层取消句柄及完成凭证需避免所有权环；关闭流程不得在 loop/worker 中阻塞等待自身退出。

### libuv 等宿主接入

CAORS 核心不包含 libuv 头、不链接 libuv，也不拥有或启动 `uv_loop_t`。
上游适配器负责 loop 线程投递、I/O 请求与缓冲区保活、backend 错误映射、取消及关闭时的迟到回调清理。
libuv 的请求清理规则由上游实现遵守；相关 API 以[官方文件系统文档](https://docs.libuv.org/en/v1.x/fs.html)为准。
测试与可运行示例先使用无第三方依赖的延迟完成后端，证明协议无需占住 CAORS worker 等待 I/O。
真实 libuv 联合验证独立记录，不用模拟测试代替已经完成接入的声明。

## 实施批次

### AL-01：协议与寿命模型

- [x] 评审启动上下文、完成凭证与底层取消访问的精确签名，形成公共中文 Doxygen 契约。
- [x] 固定 asset_manager 运行期默认调度配置、兼容构造/初始化、asset_group 创建与转发接口，以及未配置/失效/重配错误。
- [x] 固定跨 group 共享操作的首次调度归属、manager 配置代次与旧 group 失效规则，明确非拥有寿命。
- [x] 实现启动拒绝、同步完成、延迟完成、完成凭证丢弃和恰好一次交付；确定重复交付防护与错误归属。
- [x] 固定结果/缓冲区/allocator/模块所有权、回调线程、无环持有与无异常模式支持边界。

### AL-02：资源请求与 cache

- [x] 异步 load/reload 复用共享操作、独立订阅、进度与现有 coroutine 等待。
- [x] 同步 loader fallback 与异步优先路由，保持原同步入口；async-only 缓存未命中同步返回 unsupported。
- [x] 覆盖取消登记窗口、启动/完成竞态、cache 提交边界、失败时保留旧 reload 资源。
- [x] 完成 executor 拒绝/丢弃、内存分配失败及迟到结果均正确释放并收口。

### AL-03：database 与 asset_manager

- [x] 扩展类型擦除注册、manifest type/type_guid 绑定和冲突诊断，允许同步/异步能力并存。
- [x] 接通 asset_manager 类型工厂及 request_load/request_reload，保持已有同步工厂兼容。
- [x] 增加默认 worker/completion 配置和统一请求重载，覆盖加载、重载及两类预加载；保持原显式调度调用可用。
- [x] 实现 asset_group 的创建、复制/移动与请求转发；共享 manager/cache/请求表，按 runtime 配置代次拒绝旧 group 请求。
- [x] 不同 group 共享同一加载时保留首次调度组合，验证新订阅、独立取消及 cancel_shared 的跨组影响。
- [x] 请求固定发起时的调度快照，约束在途操作期间重配，明确空闲重配不迁移 cache 所属线程。
- [x] 支持同类型与跨类型 request_preload 的同步/异步混合依赖、共享加载与失败传播。
- [x] 明确重新配置/clear/销毁前的宿主清理步骤，覆盖真实在途 I/O 与回调的寿命。

### AL-04：验收、安装与文档

- [x] 提供可控延迟、同步完成、拒绝、取消后迟到与丢弃的模拟宿主，以及初始化一次默认调度、group 调度复用与三个请求语义的示例。
- [x] 跑下列契约矩阵、必要重复测试与 sanitizer，再在完整批次集中运行常规 CI。
- [x] 安装包公共头独立包含，consumer 覆盖同步兼容与异步请求/coroutine；迁移前缀后可用且不依赖 libuv。
- [x] 实现后更新 resources/assets 参考、迁移说明和 CHANGELOG，真实数据写 Record，再将 Plan 标为已完成。

## 验收场景

| 场景 | 必须观察的结果 |
| --- | --- |
| 仅启动、尚无 I/O 回调 | 发起任务已返回，worker 可执行其他工作，请求仍等待 |
| 启动拒绝、返回前完成与稍后完成 | 无丢失通知、悬空回调、重复终态或执行线程误判 |
| 同资源多订阅，一个取消或全部放弃 | 底层工作按共享规则复用，取消隔离，迟到回调清理正确 |
| 完成凭证丢弃、完成队列关闭 | 请求不会永久 pending，未提交资源与状态正确释放 |
| 解析/分配/读取失败与 reload | 传播明确错误，旧 cache 绑定及 generation 符合现有语义 |
| 跨类型混合 preload 与协程等待 | 依赖顺序、共享工作、错误传播及恢复均保持契约 |
| 关闭、重配、模块卸载前清理 | 实际 I/O 与捕获对象清理后才释放服务、allocator 与模块 pin |
| 默认调度与 group 调度复用 | manager 初始化默认值，group 初始化特殊组合，加载/重载/两类预加载均可省略 executor；原显式入口兼容 |
| 跨 group 共享与取消 | 同一底层操作只启动一次，沿用首次调度组合；订阅取消隔离，cancel_shared 按原规则影响全部订阅 |
| group 复制、移动、析构与配置失效 | 不复制 cache、不取消已发请求；清空/重配 runtime 后旧 group 不能访问新配置，manager 仍须覆盖 group 访问寿命 |
| 未配置默认值与调度环境关闭 | 无默认值的便捷入口返回失败请求，显式入口仍可用；关闭后的投递错误不被默认回退覆盖 |
| 默认调度下缓存命中、同步与异步 loader | 使用同一请求类型，支持立即完成与延迟完成，cache 均在所属线程提交 |
| 调度重配与迟到 I/O | 拒绝可检测的在途重配，不以请求终态代替实际操作已清理；请求不更换已有句柄快照 |
| 无外部 I/O 库的安装 consumer | 导出仅依赖 CAORS 公开契约，不泄漏 backend 类型 |

确切编译器、CI SHA、测试次数与结果在实际执行后记录，本计划不预填已通过数据。

## 已收口的接口取舍

- async_loader 继承同步 loader，默认同步入口返回 unsupported；注册槽区分同步/异步能力，共用 start 请求管线。
- asset_execution_options 通过构造或 set_execution_options 初始化；asset_group 固定组合与 runtime 代次，不新增组级资源管理。
- load_completion 的启动结算保留拒绝原因，complete 返回是否首次交付；std::stop_token 表达协作取消，宿主自行转发到 loop。
- 完成凭证与拥有型进度通道共享真实操作 lease；manager 重配据此拒绝 frozen，直接 database 清空及析构仍遵守宿主清理前提。
- 无异常只保留公共头编译支持，不承诺完整无异常运行矩阵；标准库 stop 状态系统分配单独说明。

实际验证、故障注入修复与完整矩阵见[v0.3.0 执行记录](../records/2026-10-19-v0.3.0-async-loader.md)。
