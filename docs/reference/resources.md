<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Resources

本文档说明 CAORS 资源系统的当前设计边界。资源系统仍处于基础阶段，当前提供资源身份、轻量引用模型、
运行期共享 handle、非拥有资源索引、引用计数缓存、资源依赖图、资源清单、字节来源 provider、
加载/保存接口、非拥有资源 database 编排入口、同步 reload、显式 reload 通知、同步预加载计划和
第一版异步加载请求；资源包后续再补。高层工程配置、asset manager 和 JSON 适配见
[assets.md](assets.md)。

## 设计目标

资源系统第一阶段只解决几件事：

- 资源有跨进程、跨存档稳定的 `resource_guid`。
- 资源对象可以保存自己的稳定身份。
- 资源引用可以先保存 GUID，在资源加载后再解析成实际资源指针。
- 运行期资源 handle 可以用引用计数共享资源对象生命周期。
- 资源管理器可以保存非拥有索引，用 GUID 定位已经存在的资源对象。
- 资源缓存可以按 GUID 持有强 handle，并复用 manager 做查找和引用解析。
- 资源依赖图可以记录资源 GUID 之间的依赖和反向依赖。
- 资源清单可以把稳定 GUID 映射到资源位置、类型、版本和 hash。
- 资源 provider 可以把稳定 GUID 映射到字节来源和位置，作为资源数据库或调试 fileserver 的运行期读取入口。
- manifest file system 可以在底层文件系统不能列目录时，用 manifest 的 location 推导目录结构。
- 资源加载器和保存器先只定义类型化接口，不绑定路径、文件系统或异步框架。
- 资源 database 可以把 manifest、provider、cache、loader 和依赖图串成一条主流程。
- 同步预加载计划可以按依赖图计算依赖优先的加载顺序，并在需要拓扑顺序的流程中检测循环依赖。
- 异步加载请求可以把后台 loader 阶段和主线程 cache commit 阶段分开。
- 底层资源模型不绑定具体引擎、文件系统、异步加载框架或编辑器管线。

当前不会实现：

- 资源包或网络加载后端。
- 拥有式全局资源缓存。
- cache 级自动淘汰、后台释放或跨 cache 生命周期协调。
- 完整热重载系统。
- 资源导入管线。
- 自动创建未注册资源类型的 cache / loader。
- 请求优先级和细粒度进度聚合。
- 与对象系统的场景树生命周期绑定。

这些能力后续应建立在 `io::file_system`、`resources::provider`、`resources::manager`、
`resources::loader` / `resources::saver` 和上层资源数据库之上。

## 模块边界

`resources` 是底层资源核心，目标是提供不绑定具体格式和工程配置的资源基础设施：

- `resource_guid`、`resource`、`ref<T>`、`handle<T>`、`cache<T>`、`manager`。
- `source`、`provider`、`manifest`、`dependency_graph`。
- `manifest_file_system` 作为 manifest 到 `io::file_system` 的目录 overlay。
- `loader<T>` / `saver<T>` 抽象、同步/异步加载组合函数和非拥有 `database`。

`resources/serialization.h` 是资源引用与序列化上下文之间的适配层。它提供 `resource_ref<T>` codec、
依赖收集器和 resolver helper；资源核心头 `caors/resources.h` 不依赖序列化。

`assets` 是高层工程资源入口，允许依赖 `resources`、`serialization`、`io` 和运行期配置。它的职责、
配置格式和 JSON 适配见 [assets.md](assets.md)。因此普通资源核心代码优先包含 `caors/resources.h`；
需要资源引用序列化适配时显式包含 `caors/resources/serialization.h`；需要工程配置和 asset manager
时包含 `caors/assets.h`；需要 JSON loader/saver、JSON manifest 或 JSON project config 时显式包含
`caors/assets/json.h`。

## 命名空间

资源系统主定义位于 `caors::resources` 命名空间，例如：

- `resources::resource_guid`
- `resources::resource`
- `resources::ref<T>`
- `resources::handle<T>`
- `resources::weak_handle<T>`
- `resources::manager`
- `resources::cache<T>`
- `resources::dependency_graph`
- `resources::database`
- `resources::manifest`
- `resources::manifest_entry`
- `resources::manifest_file_system`
- `resources::source`
- `resources::file_source`
- `resources::provider`
- `resources::loader<T>`
- `resources::saver<T>`
- `resources::request_status`
- `resources::load_request<T>`
- `resources::preload_request`
- `resources::preload_item`
- `resources::preload_plan`
- `resources::preload_result`
- `resources::request_load_into()`
- `resources::request_reload_into()`
- `resources::reload_event<T>`
- `resources::reload_signal<T>`

根命名空间只保留高频资源语义别名：

- `caors::resource_guid`
- `caors::resource_guid_hash`
- `caors::resource_generation`
- `caors::invalid_resource_generation`
- `caors::resource`
- `caors::resource_ref<T>`
- `caors::resource_handle<T>`
- `caors::resource_weak_handle<T>`
- `caors::resource_cache<T>`
- `caors::resource_dependency_graph`
- `caors::resource_database`
- `caors::resource_manifest`
- `caors::adopt_handle()`
- `caors::make_handle()`

低频组合件不再放入根命名空间，包括 source/provider、loader/saver、request/result/status、
manifest helper、reload event/signal 和资源序列化 resolver helper。需要手动拼装资源管线时，
显式使用 `caors::resources::...` 可以让职责边界更清楚；工程级入口见 `caors::assets`。

## resource_guid

`resource_guid` 是基于 `core::guid` 的强类型稳定资源标识：

```cpp
auto parsed = caors::resource_guid::parse("6cd2da3d-b549-4756-8b8b-0c84293af410");
if (parsed) {
  caors::resource_guid id = *parsed;
}
```

需要注意：

- nil GUID 表示资源尚未绑定持久身份。
- `resource_guid` 不由资源路径、资源名称或运行期 hash 自动派生。
- 序列化资源引用时应写出 `resource_guid`，而不是运行期指针。

## resource

`resource` 是资源基类，只保存稳定资源 GUID：

```cpp
class texture : public caors::resource {};

texture value;
value.set_guid(id);
```

需要注意：

- `resource` 不负责加载、缓存、引用计数或线程同步。
- 修改 GUID 时，调用方需要保证资源索引、缓存和外部引用保持一致。
- `resource` 和 `meta::object` 的边界不同：资源表示可持久化、可缓存、可复用资产；对象表示当前运行期实例。

## ref

`resources::ref<T>` 保存一个 `resource_guid`、一个可选已解析资源指针，以及解析时记录的运行期
`resource_generation`：

```cpp
caors::resource_ref<texture> reference{id};

if (!reference.resolved()) {
  // 可以由 resources::manager 按 GUID 查找已存在资源。
}
```

资源加载完成后可以显式解析：

```cpp
texture value{id};

if (reference.resolve(&value)) {
  texture* current = reference.get();
}
```

需要注意：

- `ref<T>` 不拥有资源，也不延长资源生命周期。
- `resolve()` 会拒绝已保存 GUID 和资源对象 GUID 不一致的情况。
- 通过 `manager.resolve(ref)` 或 `cache.resolve(ref)` 解析时，引用会记录当前 GUID 绑定的 generation。
- `manager.is_current(ref)` / `cache.is_current(ref)` 可用于判断已解析引用是否仍指向当前索引中的同一绑定版本。
- `unresolve()` 只清除资源指针，保留 GUID，适合资源卸载或缓存失效后继续保留引用身份。
- `resource_generation` 是当前进程内 manager/cache 的运行期版本，不会被序列化，也不能作为跨进程稳定身份。

## handle

`resources::handle<T>` 是运行期资源强引用句柄，`resources::weak_handle<T>` 是弱引用句柄：

```cpp
caors::resource_handle<texture> texture =
    caors::make_handle<texture>(texture_id);

caors::resource_weak_handle<texture> weak = texture;
if (auto locked = weak.lock()) {
  texture* current = locked.get();
}
```

需要注意：

- `handle<T>` 通过独立控制块拥有资源对象；最后一个强 handle 释放时销毁资源。
- `weak_handle<T>` 不延长资源生命周期，资源销毁后 `lock()` 返回空 handle。
- `make_handle<T>(resource, args...)` 会使用指定 `memory_resource` 构造资源对象和控制块。
- `adopt_handle<T>(unique_ptr, resource)` 可以把 loader 返回的 `memory::unique_ptr<T>` 转成强 handle。
- `handle<T>` 是运行期所有权工具，不是稳定资源引用；序列化、依赖收集和资源数据库仍应使用
  `resource_guid` / `resource_ref<T>`。
- `cache<T>` 内部持有强 handle。`erase()`、`clear()` 或 `replace()` 只释放 cache 自己的强引用；
  如果外部仍持有 `resource_handle<T>`，旧资源会继续存活到最后一个强引用释放。

## 同步预加载

`resources::dependency_graph` 记录资源 GUID 之间的依赖边：`resource -> dependency`。资源引用关系可以
出现循环，但打包、预加载和拓扑排序这类“需要依赖先于使用者”的流程必须显式检测环：

```cpp
caors::resource_dependency_graph graph;
graph.add(material_id, texture_id);

if (graph.has_cycle(material_id)) {
  auto cycle = graph.find_cycle(material_id);
}
```

`resources::database` 提供两类同步预加载入口。

明确知道整个依赖闭包都由同一个资源类型处理时，可以使用模板入口：

```cpp
caors::resources::preload_plan plan =
    database.plan_preload<texture>(texture_id);

if (plan.succeeded()) {
  caors::resources::preload_result result =
      database.preload<texture>(texture_id);
}
```

如果依赖闭包中存在不同资源类型，可以把 manifest type 绑定到对应 cache/loader，然后使用无模板入口：

```cpp
caors::string_name texture_type{"game.texture"};
caors::string_name material_type{"game.material"};

manifest.bind(texture_id, "textures/albedo.json", texture_type);
manifest.bind(material_id, "materials/body.json", material_type);

database.register_type(texture_cache, texture_loader, texture_type);
database.register_type(material_cache, material_loader, material_type);

caors::resources::preload_result result = database.preload(material_id);
```

如果外部资源数据库使用另一套类型 token，可以把外部 token 映射到已有 manifest type：

```cpp
caors::string_name editor_texture_type{"editor.texture"};

database.register_type(texture_cache, texture_loader, texture_type);
database.bind_resource_type_alias(editor_texture_type, texture_type);

manifest.bind(texture_id, "textures/albedo.json", editor_texture_type);
```

如果 manifest type 直接保存稳定 `type_guid` 的字符串形式，可以把对应 GUID 绑定到资源类型：

```cpp
caors::type_guid texture_guid{...};
caors::string_name texture_guid_token{texture_guid.value.string()};

database.register_type(texture_cache, texture_loader, texture_guid);
manifest.bind(texture_id, "textures/albedo.json", texture_guid_token);
```

当前语义：

- `plan_preload<T>(root)` 从 `root` 出发遍历依赖闭包，生成依赖优先的 `load_order`。
- `plan_preload(root)` 要求 database 已设置 manifest，会从每个 manifest 条目读取 `type` 并按
  `register_type(cache, loader, type)`、`bind_resource_type_alias()` 或 `register_type(cache, loader, type_guid)`
  绑定分派。
- `load_order` 和 `already_loaded` 保存 `resource_preload_item{guid, type}`，跨类型预加载时 type
  就是 manifest 中保存的原始 type token。
- 已经在对应已注册 cache 中的资源会进入 `already_loaded`，不会重复加载。
- 如果 database 设置了 manifest，缺失清单条目会进入 `missing_manifest`，计划返回
  `resources / not_found`。
- 如果 manifest 条目缺失 type，会进入 `missing_type`，计划返回 `resources / missing_type`。
- 如果 manifest type、外部 alias 或 type_guid token 没有对应 cache 或 loader，会进入
  `missing_cache` / `missing_loader`，计划返回 `resources / not_found`。
- 如果依赖闭包中存在环，`cycle` 保存一个闭合路径，计划返回 `resources / cycle_detected`。
- `preload<T>(root)` 会先生成计划，再按 `load_order` 调用已注册 `loader<T>` 和 `cache<T>`。
- `preload(root)` 会先生成跨类型计划，再逐项按 manifest type 分派到对应 loader/cache。

当前限制：

- 跨类型入口只解析 manifest `type` 中已经存在的 token；外部资源数据库需要先把自己的类型 token
  显式绑定到 CAORS 资源类型，不会自动访问外部数据库。
- 不会自动递归解析对象、场景或资源包层级的依赖；这些能力后续建立在当前计划结构之上。

## 异步加载请求

异步资源 API 分为发起/加载与提交两个阶段：

- 加载阶段：worker 调用 `loader<T>::start()`。同步 loader 默认适配到 `load()`；原生异步 loader 发起宿主 I/O 后返回，通过完成凭证稍后交付结果。
- 提交阶段：通过调用方指定的 completion executor 把加载结果提交到 `cache<T>`，执行 insert 或
  replace；通常由主线程 `execution_context` 提供该 executor。

直接使用 cache 和 loader：

```cpp
caors::resource_cache<texture> cache;
texture_loader loader{provider};
caors::tasks::thread_pool pool{2};
caors::tasks::executor worker = pool.get_executor();
caors::tasks::execution_context main_context;
caors::tasks::executor completion = main_context.get_executor();

auto request =
    caors::resources::request_load_into(cache, loader, texture_id, worker, completion);

// 后台任务完成后，在主线程明确时机提交 cache 修改。
main_context.poll();

if (request.loaded()) {
  caors::resource_handle<texture> texture = request.try_get();
}

auto result = request.result();
if (result.failed()) {
  caors::error status = result.error();
}
```

通过 database 使用已注册 cache/loader：

```cpp
caors::resource_database database;
database.register_type(cache, loader);

auto request = database.request_load<texture>(texture_id, worker, completion);
```

`database.request_load<T>()` 和 `database.request_reload<T>()` 会按资源类型和 GUID 复用进行中的请求：

- cache 已经有同 GUID 资源时，`request_load<T>()` 立即返回 loaded 请求，不提交后台任务。
- 同一个资源类型、同一个 GUID 的 queued/loading load 复用一次底层加载，各次调用返回独立订阅。
- 同一个资源类型、同一个 GUID 的 queued/loading reload 同样复用底层操作并返回独立订阅。
- load 和 reload 使用独立请求表；同 GUID 的首次加载和重载不会互相复用。
- `cancel()` 只取消当前订阅；普通句柄复制共享该订阅，其他独立调用不受影响。`cancel_shared()` 显式取消底层操作的全部订阅。
- 请求 loaded、failed 或 cancelled 后会从请求表移除，后续同 GUID 请求可以重新提交。

依赖预加载也可以异步提交。模板入口适合同一资源类型闭包；无模板入口会先读取 manifest type，
再按 `register_type(cache, loader, type)` 的绑定跨类型分派：

```cpp
caors::resource_database database;
database.set_manifest(manifest);
database.set_dependency_graph(graph);
database.register_type(texture_cache, texture_loader, texture_type);
database.register_type(material_cache, material_loader, material_type);

caors::resources::preload_request request =
    database.request_preload(material_id, worker, completion);

// 后台 loader 完成后，在主线程提交各个 cache 修改。
main_context.poll();

if (request.loaded()) {
  caors::resources::preload_result result = request.result();
}
```

重载使用 `request_reload_into()` 或 `database.request_reload<T>()`。重载成功时 cache 指向新资源；旧
`resource_handle<T>` 仍继续持有旧资源。重载失败时旧资源保持不变。

`load_request<T>` 状态：

- `queued`：请求已创建，但后台任务尚未开始。
- `loading`：后台任务正在加载，或加载完成后等待主线程 commit。
- `loaded`：主线程 commit 成功，`try_get()` 返回结果 handle。
- `failed`：请求失败，`error()` 返回失败原因。
- `cancelled`：请求已取消。已经开始的后台 loader 不会被强制中断，但后续 completion executor commit 会跳过，
  不再写入 cache。

常用查询：

- `cancel()`：queued/loading 状态可取消；loaded/failed/cancelled 状态返回 false。
- `done()`：loaded、failed 或 cancelled 时返回 true。
- `try_get()`：非阻塞；loaded 时返回结果 handle，未完成、失败或取消时返回空 handle。
- `result()`：非阻塞；loaded 返回成功和结果 handle，queued/loading 返回 `resources / pending`，
  cancelled 返回 `resources / cancelled`，failed 返回原始错误。
- `progress()`：非阻塞；queued/loading 返回最近一次 loader 上报的 0.0 到 1.0 进度，终态返回 1.0。
- `progress_stage()`：返回最近一次 loader 上报的阶段文本副本。

### Coroutine 等待

显式包含可选头后，资源请求可以被多个 coroutine 非阻塞等待：

```cpp
#include <caors/resources/coroutine.h>

auto request = database.request_load<texture>(texture_id, worker, completion);
caors::resources::handle_result<texture> result =
    co_await caors::resources::async_wait(request);

caors::resources::preload_result preload_result =
    co_await caors::resources::async_wait(preload_request);
```

- `caors/resources.h` 不隐式包含 coroutine 适配；不使用 coroutine 的调用方无需包含 `<coroutine>`。
- `load_request<T>` 返回 `handle_result<T>`，`preload_request` 返回 `preload_result`；失败和取消继续通过
  `caors::error` 表达。
- 已经进入 loaded、failed 或 cancelled 的请求不会挂起。
- 同一个共享请求可以注册多个等待者；请求进入终态后，在释放请求状态锁后恢复全部仍存活的等待者。
- 默认由使请求进入终态的线程恢复。正常 commit 通常在 completion executor 线程恢复；后台加载失败
  可能在 worker 线程恢复。需要固定后续线程时，再显式 `co_await tasks::schedule(target)`。
- 销毁等待 coroutine 只放弃该等待，不会取消共享资源请求；显式调用 request 的 `cancel()` 影响
  同一订阅的普通副本，不影响另一次独立请求。
- 每次等待的辅助状态使用 request 的长期 `memory_resource`；coroutine frame 仍由调用方所用 coroutine
  类型及编译器负责分配。
- request 只延长共享请求状态，不拥有 database、cache、loader、provider 或 source。销毁这些服务前，
  调用方必须停止提交并 drain 或取消在途工作。

可以使用 request poll/drain helper 在主循环或测试中收敛完成 `execution_context`：

- `poll_request(completions, request)`：执行一轮当前已经入队的完成回调，并返回请求是否完成。
- `drain_until_done(completions, request, max_drains)`：持续 drain，直到该请求完成、队列暂时为空或达到轮数上限。
- `all_requests_done(requests)`：判断一组请求是否都已进入终态。
- `poll_requests(completions, requests)` / `drain_until_all_done(completions, requests, max_drains)`：
  面向一组 `load_request<T>` 或 `preload_request` 的批量版本。

这些 helper 不阻塞等待后台线程，也不自带条件变量；如果返回 `done == false`，说明当前完成执行环境
暂时没有足够回调，调用方应在后续帧或外部等待策略后再次调用。

loader 可选择覆盖带 `progress_reporter` 的重载来上报内部阶段进度；不覆盖时默认忽略进度并转调旧
`load(guid, resource)`：

```cpp
class texture_loader final : public caors::resources::loader<texture> {
public:
  caors::resources::load_result<texture>
  load(caors::resource_guid guid, caors::memory_resource* resource) override {
    return {{}, caors::memory::make_unique<texture>(resource, guid)};
  }

  caors::resources::load_result<texture>
  load(caors::resource_guid guid,
       caors::memory_resource* resource,
       caors::resources::progress_reporter progress) override {
    progress.report(0.25, "read");
    progress.report(0.75, "decode");
    return load(guid, resource);
  }
};
```

`progress_reporter` 只在 `load()` 调用期间有效，loader 不应保存它或跨线程延后调用。

`preload_request` 是批量请求句柄，不直接保存每个资源的强 handle；成功加载的资源由对应 `cache<T>`
持有，调用方可通过 cache 或 database 查询。它额外提供：

- `total_count()`：计划中本次需要异步加载的资源数量；已缓存资源不计入。
- `loaded_count()`：已经在 completion executor 阶段成功提交到 cache 的资源数量。
- `failed_count()`：已经失败的资源数量。
- `pending_count()`：还未结束的资源数量；请求进入 loaded、failed 或 cancelled 后返回 0。
- `progress()`：非阻塞聚合进度。它按本次实际提交的资源项平均计算，未完成项会使用对应 loader 最近一次上报的内部进度。
- `progress_stage()`：返回最近一次底层 loader 上报的阶段文本副本。
- `result()`：返回本次 `preload_plan`、已经成功提交的 `loaded` 列表和失败项明细 `failed_items`；
  queued/loading 时
  `status` 为 `resources / pending`。

异步预加载会复用 database 级单资源请求表：

- 同一个资源类型、同一个 GUID 的 direct load 和 preload load 共享一次底层加载，订阅状态隔离。
- 多个 preload 请求命中同一个资源时只提交一次后台 loader；每个 `preload_request` 都会观察共享请求的最终结果。
- 计划阶段已经在 cache 中的资源会进入 `plan.already_loaded`，`total_count()` 不计入这些资源，也不会提交后台任务。
- 计划后、提交前被其它路径写入 cache 的资源会立即标记该 preload item 完成。
- 取消 `preload_request` 只取消该批量请求的聚合状态；它不会取消已经共享给其它请求的底层 `load_request<T>`。

失败策略：

- nil GUID：立即进入 `failed / resources::invalid_argument`。
- database 未注册对应 cache 或 loader：立即进入 `failed / resources::not_found`。
- loader 返回失败：进入 `failed`，错误原样保留。
- loader 返回空资源：进入 `failed / resources::backend_failure`。
- 新资源 GUID 与请求 GUID 不一致：进入 `failed / resources::inconsistent_type`。
- load commit 时 cache 已被其它路径插入同 GUID：进入 `failed / resources::already_exists`。
- reload commit 失败：进入 `failed / resources::backend_failure`，旧资源保持不变。
- executor 拒绝提交：进入 `failed`，错误来自 executor，例如 `core / frozen`。
- 调用方取消请求：进入 `cancelled / resources::cancelled`。若后台已经加载完成但主线程尚未 drain，
  completion 回调会释放加载结果并跳过 cache commit。
- 异步预加载中任一资源加载或 commit 失败，整个 `preload_request` 进入 failed；已提交成功的资源保留在
  cache 中，尚未 commit 或尚未通知该批量请求的 completion 会跳过写入或状态更新。
- `preload_result::failed_items` 保存逐项失败明细，元素为 `{preload_item, error}`。计划阶段的
  missing manifest / missing type 会使用空 `type`，missing cache / loader、loader 失败、commit
  失败和共享请求取消会保留对应 manifest type。批量请求的 `status` 仍只保存首个对外错误。

线程边界：

- `cache<T>`、`manager`、manifest、对象状态和运行期场景状态只能在主线程 commit 阶段修改。
- 后台任务可以调用 loader，但 loader、provider、source 和 memory_resource 是否能跨线程使用由调用方保证。
- `load_request<T>` 自身状态查询是线程安全的。
- `preload_request` 自身状态查询是线程安全的。
- completion executor 应由拥有 cache 的线程运行；使用手动 pump 执行环境时，对应操作是
  `execution_context::drain()`。
- execution context 不聚合完成回调自身的返回错误；异步资源请求会把 loader/commit 错误写入 request
  状态，调用方应通过 `request.result()` 或 `preload_request::result()` 获取最终错误。
- `load_request<T>::try_get()` 和 `result()` 都不阻塞；需要跨帧推进时使用
  `poll_request()`、`drain_until_done()`、`poll_requests()` 或 `drain_until_all_done()`。
- 当前异步预加载已复用同类型同 GUID 的进行中 load 请求；优先级、阻塞等待和分层依赖等待后续再补。

## 序列化

`caors/serialization.h` 包含 `resource_guid` 和 `resource_ref<T>` 的 codec。资源引用序列化只保存稳定
GUID，不展开资源对象，也不保存运行期资源指针：

```cpp
struct material {
  caors::resource_ref<texture> albedo;
};

caors::registration::type_<caors::resource_ref<texture>>("game.resource_ref<game.texture>");
```

写出规则：

- `resource_ref<T>` 为空或 GUID 为 nil 时写出 `null`。
- 非空引用写出 `resource_guid` 字符串。
- 即使引用已经解析到资源对象，也只写 GUID，不写资源对象内容。

读回规则：

- `null` 会清空引用。
- GUID 字符串会读回为未解析的 `resource_ref<T>`。
- 默认读回不会访问全局 manager、cache 或 locator，避免序列化层隐式决定资源生命周期。

资源加载完成后再显式解析：

```cpp
material decoded{};
caors::read_json_from(decoded, R"({"albedo":"6cd2da3d-b549-4756-8b8b-0c84293af410"})");

caors::resource_cache<texture> cache;
// cache 创建或加载同 GUID 的资源后：
cache.resolve(decoded.albedo);
```

也可以通过 `serialization::context` 为本次反序列化挂一个通用外部引用 resolver。资源模块提供
`resources::make_resource_resolver()` 适配 `resources::manager`、`resources::database` 或
`resources::cache<T>`：

```cpp
caors::resource_cache<texture> cache;
texture* loaded = cache.create(texture_id);

caors::resources::resource_ref_resolver_ops resolver = caors::resources::make_resource_resolver(cache);

caors::serialization::context context{};
context.resolvers.external_refs.resolver = &resolver;
context.resolvers.external_refs.resolution = caors::resources::resource_ref_resolution::resolve_if_present;

material decoded{};
caors::read_json_from(context, decoded, R"({"albedo":"6cd2da3d-b549-4756-8b8b-0c84293af410"})");
```

`context.resolvers.external_refs.resolution` 的策略：

- `keep_unresolved`：默认策略，只恢复 GUID，不解析资源。
- `resolve_if_present`：如果 resolver 能找到同 GUID 且类型兼容的资源，则写入已解析指针；缺资源仍保持 unresolved。
- `require_resolved`：必须解析成功；resolver 缺失返回 `serialization / invalid_argument`，资源缺失返回 `resources / not_found`，类型不匹配返回 `resources / invalid_type`。

序列化和反序列化遍历 `resource_ref<T>` 时还可以收集资源依赖：

```cpp
caors::resources::resource_dependency_list dependencies;
caors::resources::resource_dependency_collector_ops collector =
    caors::resources::make_resource_dependency_collector(dependencies);

caors::serialization::context context{};
context.resolvers.external_refs.collector = &collector;

caors::to_json(context, material_value);
```

依赖条目包含 `resource_guid` 和资源声明类型的 `type_id`。这可用于后续预加载、打包、热更新和资源数据库。
依赖收集器本身不加载资源，也不改变引用的 resolved 状态。常见同步使用方式是：先从对象或存档中收集
`resource_ref<T>` 依赖，再按 GUID 查询 manifest / provider，使用匹配的 loader 预加载到 `cache<T>`，
最后把 cache 适配成 `resource_ref_resolver_ops`，让反序列化阶段按策略解析引用。

## manager

`resources::manager` 是非拥有资源索引。它保存 `resource_guid -> resource*` 映射，可以绑定、查找、
解绑和清空资源，也可以把 `resource_ref<T>` 解析成实际资源指针：

```cpp
caors::resources::manager manager;

texture value{id};
manager.bind(&value);

caors::resource_ref<texture> reference{id};
if (manager.resolve(reference)) {
  texture* current = reference.get();
}
```

需要注意：

- `manager` 不创建、不销毁资源，也不延长资源生命周期。
- `bind(guid, resource)` 在资源对象 GUID 为 nil 时会写入传入 GUID；如果资源对象已经有不同 GUID，则绑定失败。
- 同一对 GUID 和资源重复绑定视为成功；同一个 GUID 或同一个资源指针绑定到不同对象会失败。
- `unbind()` 和 `clear()` 只清理索引，不修改资源对象自身 GUID。
- 跨线程访问时调用方需要自行同步。
- 后续拥有式缓存、加载、卸载、失效通知和异步策略应在 manager 之上继续扩展，而不是由当前非拥有索引隐式完成。

## cache

`resources::cache<T>` 是拥有式资源缓存。它按 `resource_guid` 创建并持有 `resources::handle<T>`，
同时维护一个内部 `resources::manager` 作为非拥有索引：

```cpp
caors::resource_cache<texture> cache;

texture* value = cache.create(id);
caors::resource_handle<texture> held = cache.find_handle(id);
caors::resource_ref<texture> reference{id};

if (cache.resolve(reference)) {
  texture* current = reference.get();
}
```

需要注意：

- `cache<T>` 内部保存强 handle。`create_handle()`、`find_handle()`、`release()` 和
  `load_handle_into()` 返回强 handle；`create()` 和 `find()` 只是返回非拥有裸指针视图。
- `insert()` 可以接收已经构造好的 `memory::unique_ptr<T>` 或 `resource_handle<T>`，适合 loader
  把资源所有权转入缓存。
- `replace()` 可以替换同 GUID 的已有资源，适合同步 reload 成功后把新资源放入 cache。旧资源会在
  最后一个外部强 handle 释放后销毁。
- `release()` 会从缓存和索引中移除资源，并把强 handle 交给调用方。
- `create_handle(guid, args...)` 会使用缓存的 `memory_resource` 构造资源；nil GUID、重复 GUID
  或构造后资源 GUID 与传入 GUID 不一致都会失败。
- `cache<T>` 当前不负责从磁盘、网络或资源包加载内容；loader 层会决定如何构造资源。
- `cache<T>` 不提供内部线程同步；跨线程访问时调用方需要自行加锁。
- `erase()`、`release()`、`clear()` 和 `replace()` 不会通知外部已解析的 `resource_ref<T>` 或裸指针。
  已解析引用可能继续保存旧资源地址；调用方可以用 `cache.is_current(ref)` 检测是否仍为当前版本，并在资源
  卸载或替换后重新解析引用，或由上层系统广播失效事件。需要保证旧资源存活时，应保存
  `resource_handle<T>`，不要只保存裸指针。

## dependency_graph

`resources::dependency_graph` 记录资源 GUID 之间的依赖和反向依赖关系：

```cpp
caors::resource_dependency_graph graph;

graph.add(material_id, albedo_texture_id);
graph.add(material_id, normal_texture_id);

caors::vector<caors::resource_guid> dependencies = graph.dependencies(material_id);
caors::vector<caors::resource_guid> affected = graph.affected_by(albedo_texture_id);
caors::vector<caors::resource_guid> affected_all = graph.affected_closure(albedo_texture_id);
```

需要注意：

- `dependency_graph` 只保存 GUID 关系，不加载资源、不拥有资源对象，也不访问 cache 或 provider。
- `dependencies(resource)` 返回资源直接依赖的 GUID 列表。
- `dependents(resource)` 返回直接依赖该资源的 GUID 列表，适合 reload 后查询哪些资源需要重新处理。
- `affected_by(resource)` 是面向 reload 的语义别名，当前等价于 `dependents(resource)`，表示该资源变化后
  直接受影响的资源。
- `affected_closure(resource)` 沿反向依赖递归展开，返回所有直接或间接受影响的资源。结果会去重，循环依赖不会
  死循环，并且不会包含传入的 `resource` 自身。该查询适合 reload 级联刷新、预加载、打包和资源数据库分析。
- 依赖查询结果当前不保证稳定顺序；调用方不应依赖返回列表顺序表达优先级。
- `resources::replace_resource_dependencies(graph, resource, dependencies)` 可以把一次序列化遍历收集到的
  `resource_dependency_list` 写入依赖图。依赖条目中的 `type_id` 仍保留给 manifest、资源数据库或调试诊断使用；
  `dependency_graph` 当前只保存 GUID 关系。
- 当前第一版只建模资源到资源的直接依赖。对象、场景、资源包和编辑器导入任务可以在上层资源数据库中继续扩展。

## 高级用法：手动组合资源管线

普通项目入口优先使用 `assets::asset_manager`，由它持有 manifest、provider、dependency graph
和内部 database。`resources::database` 是高级组合入口：它不会创建全局单例，也不会拥有
manifest、provider、cache、loader 或 dependency graph；调用方把这些组件挂进去后，database
只负责把常见同步流程串起来：

```cpp
caors::resource_manifest manifest;
caors::resources::provider provider;
caors::resource_dependency_graph graph;

caors::resource_database database;
database.set_manifest(manifest);
database.set_dependency_graph(graph);

caors::resources::sync_manifest(provider, source, manifest);

caors::resource_cache<texture> cache;
texture_loader loader{provider};
database.register_type(cache, loader);

auto loaded = database.load<texture>(texture_id);
auto resolver = caors::resources::make_resource_resolver(database);
```

常用入口：

- `set_manifest()` / `set_dependency_graph()`：挂接外部组件，不接管生命周期。
- `set_snapshot_capture()`：配置通用批次数据捕获，不绑定 provider。
- 清单到 provider 的绑定使用 `manifest_binding.h` 的 `bind_manifest` / `sync_manifest` / `replace_manifest` 自由函数，不通过 database。
- `register_cache<T>()` / `register_loader<T>()` / `register_type<T>()`：按已注册 meta 类型挂接类型化
  cache 和 loader。
- `bind_resource_type_alias()` / `bind_resource_type_guid()`：把外部类型 token 或稳定 type_guid 映射到已注册
  cache/loader，供跨类型预加载分派使用。
- `load<T>(guid)` / `load_handle<T>(guid)`：通过已注册 loader 加载资源并转入对应 cache。
- `reload<T>(guid)` / `reload_handle<T>(guid)`：重新加载并替换对应 cache 中已有资源；失败时仍保留旧资源。
- `find<T>(guid)` / `find(guid)`：查询已注册 cache 中的资源。
- `resolve(ref<T>&)` / `is_current(ref<T>)`：通过已注册 cache 解析资源引用并检查 generation 是否仍当前。
- `replace_dependencies()` / `affected_by()` / `affected_closure()`：把依赖收集结果写入当前依赖图，并查询
  reload 影响范围。

需要注意：

- `database` 是编排器，不是拥有式全局资源缓存。资源生命周期仍由 `cache<T>` / `handle<T>` 决定。
- 手动组合适合嵌入已有引擎资源系统、定制 cache/loader/provider、测试资源管线或需要绕过
  `asset_manager` 工厂配置的场景。
- 普通 `load<T>()` 不会根据 manifest 的 `type` 字符串自动选择 loader；调用方仍需要明确使用模板类型。
  跨类型预加载入口会按 `register_type<T>(cache, loader, type)` 的 manifest type 绑定分派。
- `resources::make_resource_resolver(database)` 位于 `caors/resources/serialization.h`，只返回
  `resource*`；`resource_ref<T>` 的类型兼容检查仍由序列化层的
  `dynamic_cast<T*>` 完成。
- 如果希望 `resource_ref<T>` 记录 cache generation，应在读回后调用 `database.resolve(ref)` 或
  `cache.resolve(ref)`；序列化 resolver 本身只负责把 GUID 解析为资源对象。
- 跨线程修改 database、cache、provider 或 manifest 时，调用方需要自行同步。

## 高层 assets 入口

工程资源配置、`assets::asset_manager`、source/type factory、JSON loader/saver 和
`load_serialized()` / `save_serialized()` helper 已拆到 [assets.md](assets.md)。资源核心文档只描述
`resources` 底层组件；项目入口优先阅读 assets 文档。

## reload 策略现状

当前资源 reload 是一套底层同步方案，而不是完整热重载系统。它的职责划分如下：

- `resource_ref<T>` 保存稳定 `resource_guid`、可选已解析 `T*` 和运行期 generation，不拥有资源。
- `resources::cache<T>` 持有资源强 handle，并维护 GUID 到资源指针的索引。
- `resources::manifest` / `resources::provider` / `resources::loader<T>` 负责定位和重新加载资源内容。
- `resources::reload_handle_into(cache, loader, guid)` 只负责“重新加载已有资源并替换 cache 中的对象”。
- `resources::reload_and_notify(cache, loader, guid, signal)` 在 `reload_handle_into()` 之后同步广播
  `reload_event<T>`，让上层决定是否刷新引用或记录错误。

当前方案优点：

- 身份稳定：引用保存 GUID，不依赖路径或运行期地址。
- 失败安全：重新加载失败、解析失败或 GUID 不一致时，cache 保留旧资源。
- 职责清晰：底层资源库不隐式修改业务对象、场景树或编辑器状态。
- 易于扩展：后续可以在同一层之上增加通知、异步任务和依赖图。

当前方案限制：

- `resource_ref<T>` 已解析后保存的是普通指针；`cache.replace()` 切换到新资源后，旧指针不会自动置空。
  如果没有外部 `resource_handle<T>` 持有旧资源，旧指针会悬空。调用方可以用 `cache.is_current(ref)`
  检测该引用是否已经过期，但不能继续解引用旧指针。
- 调用方需要在 reload 成功后重新解析引用；可以使用 `reload_and_notify()` 显式广播资源失效/重载事件。
- 当前 generation 只能判断同一个 manager/cache 内的运行期绑定是否仍当前，不解决跨进程版本、资源依赖或
  自动刷新。
- 当前依赖图只记录资源 GUID 之间的关系；材质、场景或对象如何跟随刷新仍由上层资源数据库或运行期系统决定。
- 当前没有文件监听、完整热重载调度器和自动级联刷新；异步加载请求只覆盖 loader 后台执行和主线程
  cache commit。

主流资源系统通常会在这个基础上继续补几层能力：

1. 使用稳定资源 ID、GUID 或路径作为长期身份。
2. 由 cache、manager 或 resource database 统一拥有资源生命周期。
3. reload 失败时保留旧资源，避免运行期对象进入坏状态。
4. 资源句柄保存 generation / version 或内部控制块，用于检测旧引用是否失效。
5. reload 成功后广播资源更新事件，让使用方主动刷新运行期状态。
6. 维护资源依赖图，用于预加载、打包、热更新和级联刷新。

因此当前方案适合作为 CAORS 资源系统第一层：它先保证同步加载、替换、失败保留和显式通知语义明确；
完整热重载、自动刷新和级联刷新应在后续的文件监听、资源数据库和上层运行期系统中继续扩展。

## manifest

`resources::manifest` 是资源 GUID 到资源位置和轻量元数据的清单。它适合承载本地资源数据库、资源包索引
或调试 fileserver 下发的 manifest：

```cpp
caors::resource_manifest manifest;

manifest.bind(texture_id,
              "/assets//textures/albedo.json",
              caors::string_name{"game.texture"},
              "content-hash",
              12);

manifest.set(texture_id,
             "assets/textures/albedo-v2.json",
             caors::string_name{"game.texture"},
             "new-content-hash",
             13);
```

清单条目包含：

- `guid`：稳定资源 GUID。
- `location`：规范化后的资源位置；可以是普通虚拟路径，也可以是带 scheme 的逻辑 URI，例如 `caors://assets/a.png`
  或 `pack://base/a.png`。
- `type`：可选资源类型名，主要用于调试、导入和上层资源数据库。
- `hash`：可选内容 hash、etag 或版本指纹。
- `version`：可选递增版本号，适合热更新和调试 fileserver 判断资源是否变化。

可以把清单批量绑定到 provider：

```cpp
caors::io::memory_file_system files;
caors::resources::file_source source{files};
caors::resources::provider provider;

caors::resources::bind_manifest(provider, source, manifest);
caors::resources::sync_manifest(provider, source, manifest);
caors::resources::replace_manifest(provider, source, manifest);
```

`resources` 核心只保存 manifest 数据，不绑定具体配置格式。如果项目使用 `assets` JSON adapter，
也可以从 JSON 文本或 source 读取清单：

```cpp
caors::resource_manifest manifest;

caors::assets::json::load_manifest(manifest, R"({
  "resources": [
    {
      "guid": "6cd2da3d-b549-4756-8b8b-0c84293af410",
      "location": "assets/textures/albedo.json",
      "type": "game.texture",
      "hash": "content-hash",
      "version": 12
    }
  ]
})");

caors::assets::json::load_manifest(manifest, source, "manifest.json");
```

JSON 清单支持两种根格式：

```json
[
  {
    "guid": "6cd2da3d-b549-4756-8b8b-0c84293af410",
    "location": "assets/textures/albedo.json"
  }
]
```

```json
{
  "resources": [
    {
      "guid": "6cd2da3d-b549-4756-8b8b-0c84293af410",
      "location": "assets/textures/albedo.json"
    }
  ]
}
```

需要注意：

- `manifest` 只保存清单数据，不读取字节、不构造资源对象，也不拥有 source。
- `bind()` 会拒绝 nil GUID、空位置和重复 GUID，并会按 `io::normalize_location()` 规则规范化
  location。
- manifest 不解释 location 对应的真实后端。普通相对路径由当前 provider/source 解释；带 scheme 的
  location 应交给 `io::file_system_resolver` 或上层 source adapter 解析。
- `set()` 会新增或替换同 GUID 条目，适合资源数据库刷新和调试 manifest 热更新。
- JSON 导入要求每个条目至少包含字符串 `guid` 和字符串 `location`；`type`、`hash`、`version` 可选。
- `assets::json::load_manifest()` 的 DOM、文本和 source 重载会向现有 manifest 新增或替换条目，
  不会隐式清空已有内容。
- `bind_manifest()` 不回滚已经成功绑定到 provider 的条目；失败时调用方需要按自身策略处理。
- `sync_manifest()` 会更新 provider 中已有 GUID 的 source/location，也会新增缺失 GUID；它不会删除清单外的旧绑定。
- `replace_manifest()` 会先清空 provider，再绑定完整清单，适合完整清单刷新；失败时不会恢复旧 provider 状态。
- 清单的 `type`、`hash`、`version` 只是轻量元数据，不替代资源对象本身的序列化内容。
- 跨线程更新 manifest 或 provider 时，调用方需要自行同步。

## manifest_file_system

`resources::manifest_file_system` 是一个 `io::file_system` overlay。它用 manifest 中的 `location`
推导目录结构，字节读取和写入仍转发给底层文件系统：

```cpp
caors::io::memory_file_system storage;
caors::resource_manifest manifest;

manifest.bind(texture_id, "assets/textures/albedo.json");

caors::resources::manifest_file_system files{manifest, storage};
auto entries = files.list("assets/textures");
```

它主要用于 HTTP/CDN、调试 fileserver、资源包索引等“不一定能直接列目录”的后端：

- `list(directory)` 从 manifest 的所有 `location` 推导直接子项。
- `status(path)` 在底层不支持或找不到时，会退回到 manifest 推导的文件/目录状态。
- `exists(path)` 同时检查底层文件系统和 manifest 推导结果。
- `read_all()` / `open_read()` / `write_all()` / `open_write()` 只转发到底层文件系统，不修改 manifest。
- overlay 不拥有 manifest 或底层文件系统，调用方需要保证生命周期。
- manifest 只提供索引视图；如果实际字节不存在，读取仍会返回底层文件系统的错误。
- overlay 只从 location 推导目录树，不关心该位置最终来自 native、memory、HTTP、资源包还是调试
  fileserver。需要混合多种来源时，推荐让底层文件系统使用 `io::file_system_resolver`。

## source / provider

`resources::source` 是“按 `io::location_view` 资源位置读取字节”的接口；`resources::provider` 是
`resource_guid -> (source, location)` 的运行期索引：

```cpp
caors::io::native_file_system native;
caors::resources::file_source source{native};
caors::resources::provider provider;

provider.bind(texture_id, source, "assets/textures/albedo.png");

caors::io::byte_buffer_result bytes = provider.read(texture_id);
caors::io::input_stream_result stream = provider.open_read(texture_id);
caors::io::output_stream_result output = provider.open_write(texture_id);
caors::error written = provider.write(texture_id, std::string_view{"new-bytes"});
```

`resources::file_source` 把任意 `io::file_system` 适配成资源字节来源。本地目录、挂载文件系统和内存文件系统
可以先走这条路径。

资源包、HTTP fileserver、对象存储或其他远程后端不一定需要先伪装成 `io::file_system`。它们也可以直接实现
`resources::source`，在 `open_read()` / `open_write()` 中返回自己的流对象；provider 只保存
`resource_guid -> (source, location)`，不会关心 location 是本地路径、包内路径、URL 还是后端私有 key。
这样 loader 只依赖 `provider.open_read(guid)`，不会绑定具体来源。

需要注意：

- `provider` 不拥有 source，调用方需要保证 source 生命周期。
- `source_location::location` 是非拥有 `io::location_view`，provider 修改或销毁后失效。
- `provider` 只负责定位、读取和可选写入字节，不负责解码资源对象。
- `source::open_read()` 默认通过 `read()` 建立内存输入流；真正支持分块读取的 source 可以覆盖该函数。
- `source::open_write()` 默认建立内存缓冲输出流，并在 `close()` 时调用 `write()` 提交。
- `file_source::open_read()` 会转发到底层 `io::file_system::open_read()`。
- `file_source::open_write()` 会转发到底层 `io::file_system::open_write()`。
- 资源包或远程 source 可以覆盖 `open_read()` 直接做分块读取，也可以覆盖 `open_write()` 实现上传、
  临时文件提交或调试 fileserver 回写。
- `source::write()` 默认返回 `errc::unsupported`；只读 source 的缓冲输出流会在 `close()` 时返回该错误。
- `bind()` 会拒绝 nil GUID、空 location 和重复 GUID。
- provider 未命中 GUID 返回 `error_domain::resources / errc::not_found`。
- 具体 source 读取失败时，错误域保持来源模块语义，例如 `file_source` 可能返回 `error_domain::io`。
- 跨线程访问 provider 或 source 时，调用方需要自行同步。

## loader / saver

### 原生异步 loader

迁移时已有同步 loader 源码无需修改；本次为 loader 增加虚函数并扩展相关类型布局，
不兼容旧二进制。库、宿主与所有使用这些头文件的插件应一起重新编译，不能混用旧对象文件或 SDK。

`resources::async_loader<T>` 继承 `loader<T>`，覆盖 `start(async_load_context, load_completion<T>)`，
无需实现同步 load；其默认同步 load 返回 `resources / unsupported`，不会阻塞等待宿主 loop。
`database.register_loader()` / `register_type()` 有异步能力重载，可与同类型的同步 loader 同时注册。
异步请求优先异步能力，只有同步能力时在 worker 适配；失败不自动改用另一 loader 重试。
同步加载仍使用同步能力；async-only 的已缓存资源可通过同步 load_handle 获取，缓存未命中返回 unsupported。
同步 preload 遇到 async-only 的未缓存项也返回 unsupported，调用方可统一改用 request_preload。

`async_load_context` 提供 GUID、非拥有结果 resource、worker、协作取消的 `std::stop_token` 与拥有状态的 `load_progress`。
context 和进度通道可跨异步边界保存；原 `progress_reporter` 仍只在同步 load 调用期间有效，不能留到 I/O 回调使用。
结果 resource 必须覆盖结果与回调寿命；分配/释放跨线程时应使用支持相应并发访问的资源。

`load_completion<T>` 为 move-only 完成凭证，`complete(load_result<T>)` 接管资源或错误，交付成功返回 true；
重复交付返回 false 并销毁多余结果，成功却不含资源会归一化为 backend_failure。
接受操作后应执行或销毁凭证；未交付即销毁会使请求以 cancelled 失败收尾，不能永久保留 pending。
库内部在 start 返回后结算交付，兼容同步完成、并发提前完成及拒绝路径，保留明确的启动拒绝原因。
start 失败不得继续持有凭证或回调；异常映射为 out_of_memory 或 backend_failure。
同一凭证的移动/销毁不能与成员调用并发；完成回调允许重入并释放其所属对象。

取消单订阅不干扰其他消费者；最后订阅放弃或 cancel_shared() 会触发底层操作的 stop 信号。
预加载批次取消、失败或最后一个批次句柄释放时，只释放该批次的订阅；剩余独立请求/其他批次仍可复用加载。
宿主负责把取消转发到自己的 loop，信号不代表 I/O 已结束。迟到结果被清理，不恢复已取消订阅；
cache 提交仍遵循原线性化边界，开始提交后不承诺回滚。
错误/取消可能在产生终态的线程通知等待方，完成 executor 只保证 cache 提交的执行位置，不提供协程恢复线程亲和性。

`database.has_active_operations()` 覆盖启动队列、完成凭证、拥有型进度通道及 cache 提交任务的实际持有期。
计数用于检测库内仍持有的操作，归零也不能代替宿主确认 loop、回调和线程已经排空。
请求终态不等于该值为 false：宿主回调结束后应释放凭证与 context/进度通道，再清理 backend、allocator 或模块 pin。
直接 clear_registered_types() 仍要求调用方先清理实际在途工作；它递增 configuration_generation() 使旧 group 失效。
标准库 stop 状态内部的系统分配不保证由 CAORS resource 统计。

完整示例见 [assets_native_async.cpp](../../examples/beta/assets_native_async.cpp)。示例模拟延迟事件，
不代表 libuv 实际接入；CAORS 不包含 libuv 类型或依赖，loop 线程投递、请求清理和缓冲区由宿主适配器管理。

### 同步 loader 与 saver

`resources::loader<T>` 和 `resources::saver<T>` 是类型化资源 IO 接口。它们只描述“按 GUID 加载一个
资源”和“保存一个已有资源”，不规定路径、文件格式、资源包、缓存策略或异步调度：

```cpp
class texture_loader : public caors::resources::loader<texture> {
public:
  caors::resources::load_result<texture>
  load(caors::resource_guid id, caors::memory_resource* resource) override;
};
```

加载结果使用 `resource_load_result<T>`，内部是 `result<memory::unique_ptr<T>>`。加载器返回所有权后，
可以通过 `load_handle_into()` 转入 `resources::cache<T>` 并拿到强 handle：

```cpp
caors::resource_cache<texture> cache;
texture_loader loader;

caors::resources::handle_result<texture> held =
    caors::resources::load_handle_into(cache, loader, id);
```

保存方向可以使用 `save_from()` 从 cache 取出指定 GUID 的资源并交给 saver：

```cpp
texture_saver saver;
caors::error saved = caors::resources::save_from(cache, saver, id);
```

`load_handle_into()` 的规则：

- cache 已经有同 GUID 资源时直接返回已有对象，不再次调用 loader。
- loader 返回的资源如果没有 GUID，会写入请求 GUID。
- loader 返回的资源如果携带不同 GUID，会返回 `errc::inconsistent_type`。
- 成功后资源所有权转入 cache，`load_handle_into()` 返回强 handle。需要临时指针时可从 handle
  调用 `.get()`，但调用方不应把该裸指针当成生命周期凭据保存。

`reload_handle_into()` 用于同步重载 cache 中已经存在的资源：

- GUID 为 nil 时返回 `resources / invalid_argument`。
- cache 未包含该 GUID 时返回 `resources / not_found`；首次加载应继续使用 `load_handle_into()`。
- loader 加载失败、返回空指针或返回不同 GUID 时，不修改 cache 中旧资源。
- 只有新资源完整加载且身份校验通过后，才通过 `cache.replace()` 替换旧资源。
- 替换成功后 cache 指向新资源；外部旧 `resource_handle<T>` 会继续持有旧资源，直到最后一个强引用释放。
- 外部已解析到旧资源的 `resource_ref<T>` 或裸指针不会自动置空。使用方可以先通过
  `cache.is_current(ref)` 判断引用是否过期，再重新调用 `cache.resolve(ref)`，或由上层资源系统统一处理
  引用失效通知。

如果需要同步通知，可以使用 `reload_and_notify()`：

```cpp
caors::resources::reload_signal<texture> reloaded;

auto connection = reloaded.connect(listener);
auto result = caors::resources::reload_and_notify(cache, loader, texture_id, reloaded);
```

`reload_and_notify()` 的规则：

- 内部先调用 `reload_handle_into()`，然后无论成功或失败都会发出一次 `resource_reload_event<T>`。
- `event.status` 表示本次 reload 结果；`event.succeeded()` 为 true 时 `event.resource` 指向 cache
  中的新资源。
- 失败时 `event.resource` 为 `nullptr`，cache 中旧资源保持不变。
- 通知是同步的，不做线程切换，也不会自动刷新外部 `resource_ref<T>`；监听者可以调用
  `cache.is_current(ref)` 和 `cache.resolve(ref)` 自行刷新。

`save_from()` 的规则：

- GUID 为 nil 时返回 `errc::invalid_argument`。
- cache 未包含该 GUID 时返回 `errc::not_found`。
- 具体保存目标、格式和覆盖策略由 saver 实现决定。

推荐同步加载链路是：

1. `resources::manifest` 保存 `resource_guid -> location/type/hash/version`。
2. `bind_manifest()` / `sync_manifest()` / `replace_manifest()` 把 manifest 批量绑定到 provider。
3. `resources::provider` 只负责按 GUID 定位并读取字节。
4. `resources::loader<T>` 负责把字节解码为资源对象。
5. `load_handle_into(cache, loader, guid)` 把加载结果转入拥有式 cache。
6. `resources::resource_ref_resolver_ops` 从 manager/database/cache 查询已加载资源，用于解析
   `resource_ref<T>`。

如果对象存档中包含资源引用，推荐在读回前先做一遍依赖收集和预加载：

1. 用 `resources::resource_dependency_list` 收集所有 `resource_ref<T>` 依赖。
2. 对资源对象本身做依赖索引时，调用
   `resources::replace_resource_dependencies(graph, current_resource_guid, dependencies)` 写入依赖图。
3. 通过 manifest 查找每个 GUID 对应的资源位置、类型和版本信息。
4. 通过 provider 读取资源字节，并用对应 loader 解码。
5. 使用 `load_handle_into(cache, loader, guid)` 把资源放入 cache。
6. 反序列化对象时把 cache resolver 挂到 `context.resolvers.external_refs.resolver`，按
   `resolve_if_present` 或 `require_resolved` 决定缺资源策略。

同步重载推荐流程：

1. 更新 manifest 条目的 `location`、`hash` 或 `version`。
2. 使用 `sync_manifest(provider, source, manifest)` 更新 provider 中的 GUID 到位置映射。
3. 调用 `reload_handle_into(cache, loader, guid)` 或 `reload_and_notify(cache, loader, guid, signal)`
   重新读取 provider、解码资源并替换 cache 中旧资源。
4. 如果重载失败，`reload_handle_into()` 保留 cache 中旧资源，调用方可以继续使用旧资源并返回诊断错误。
5. 替换成功后，可以用 `dependency_graph.affected_by(guid)` 查询直接受影响的资源，或用
   `dependency_graph.affected_closure(guid)` 查询递归受影响的资源。
6. 调用方需要重新解析相关 `resource_ref<T>`，或在上层系统里发出资源失效/重载通知。

provider 本身不直接作为 `resource_ref<T>` resolver，因为它只知道字节位置，不知道如何构造具体 `T`。
如果需要“缺失时自动加载”，应在上层实现一个自定义 resolver：内部先查询 cache，未命中时通过
provider + loader 加载，再把结果插入 cache。

需要注意：

- 非默认构造、需要外部依赖或需要异步调度的资源应实现自定义 `loader<T>`。
- 基于 serialization reader/writer 的 `assets::load_serialized<T>()`、`assets::save_serialized<T>()`、
  `assets::json::loader<T>` 和 `assets::json::saver<T>` 属于高层 assets adapter，见
  [assets.md](assets.md)。

底层资源测试位于 `tests/resources/`，高层 assets 配置、JSON loader/saver 和 asset manager 测试位于
`tests/assets/`。测试组织规则见 [tests/README.md](../../tests/README.md)。

## 模块卸载与资源寿命

资源句柄不自动持有动态库租约。宿主必须显式保留 `plugin_library_lease`，覆盖资源析构函数、
最后弱控制块的 allocator、后台任务和完成回调；最后强引用释放不表示模块可以卸载。
allocator 对象本身也必须存活，只有代码模块被 pin 并不能延长已被 shutdown 销毁的 allocator。
资源尚在使用时，宿主卸载入口应拒绝或延迟请求；停止提交并排空工作后，再释放强引用、弱引用、
回调和 allocator，最后释放模块 pin。pin 由宿主作用域持有，避免插件对象反向持有自己的宿主所有者。
具体步骤见[插件集成指南](../guides/plugin-integration.md)。

全部独立订阅取消后，未开始的加载或 cache 提交会跳过；已经进入提交临界点的 cache 写入不回滚。
普通 `load_request<T>` 句柄析构不等于取消，进行中的操作会保留订阅状态至终态，调用方需要停止需求时显式取消。
后台/完成队列接受任务后再销毁它，会使请求进入失败终态，错误为 `cancelled`；
拒绝投递则保存 executor 的原始错误。通知发生在终态产生线程，协程恢复仍按指定 executor 投递。

`preload_request` 每次调用生成独立聚合状态，底层单资源加载仍复用。调用方应保留聚合句柄直到结束；最后聚合句柄释放后，该聚合的预加载需求会被放弃，不影响其他仍有效订阅。


## 加载来源快照

`source::freeze_routes()` 是可选拥有式来源扩展；默认 unsupported。
`file_source(shared_ptr<file_system>, lifetime)` 持有底层后端和宿主凭据；旧引用构造保留同步借用行为。
引用构造可捕获内置路由根，但其全部边仍必须拥有后端；借用叶根无法变成安全异步来源。

`provider.snapshot()` 复制 GUID/位置并冻结全部文件路由，结果独立于原 provider/source 和位置字符串。
捕获时禁止并发修改 provider/source 配置；后续只读解析可跨线程。无法捕获的来源按条目保留失败，
`provider_snapshot::resolve(guid)` 返回诊断，不隐式退回原借用 source。旧同步 loader 可继续使用原 provider。
单次捕获共用嵌套路由节点；同步读取也可使用解析结果 backend/path，保持与异步读取相同来源。

`async_load_context` 仅持有 `load_snapshot` 基类，不要求文件系统。VFS loader 显式调用
`snapshot_as<resources::provider_snapshot>()`，为空或类型不匹配时返回空指针；应先检查再解析。
基类只保证共享保活，不自动冻结自定义对象的外部数据。database 不持有或选择 provider。

低层 `database::set_snapshot_capture(snapshot_capture)` 可配置宿主批次捕获策略；
捕获器返回空快照不触发回退；传入空捕获器关闭快照捕获。
asset_manager 为兼容现有 VFS 接入默认安装工程捕获策略；自定义策略清空后，若需恢复，
显式调用 `project_builder{assets}.use_provider_snapshot()` 并检查结果。
配置和捕获在发起线程进行，捕获器不得重入或修改加载配置；活动操作未清理时设置返回 frozen。
捕获异常沿发起调用传播，沿用既有 provider 捕获的异常边界，不自动转换为完成回调错误。
捕获器自身可持有宿主凭据，不得形成所有权环；已经交付的快照独立保活。
此入口为底层装配原型，尚未提供新的 asset_manager 配置外观。

单次新 load/reload 在缓存与请求合并判断之后捕获；refresh 独立捕获；一次预加载闭包共用一次捕获结果。
已经合并到在途操作的订阅者继承原操作快照，不为每个订阅者重新捕获。

安装工程 provider 捕获策略时，新异步操作在排队前捕获来源，传入 `async_load_context.snapshot`。
预加载根和依赖共用一份快照；缓存命中和同 GUID 在途共享不重新选择来源，显式 reload 的新操作才捕获新状态。
不同预加载批次重用已有在途 GUID 时，仍遵守首次操作快照的例外。
独立 `request_load_into` 未配置批次数据时该字段为空；自定义 loader 需要快照时应检查并明确失败。
旧 loader 忽略上下文后继续读取实时 provider 时，不获得新的快照保证。
内置 `assets::json::loader<T>` 仍属于同步适配器，使用实时 provider；要求来源冻结的加载应使用
读取 `context.snapshot_as<resources::provider_snapshot>()` 的 loader（见下方示例），不能把将同步 loader 排入 worker 等同于 VFS 异步读取。

内置 asset_manager 文件系统工厂和来源工厂提供拥有式后端，配置路由在挂载前检查循环。
自定义 factory 可通过 `bind_file_system(name, shared_ptr<file_system>, lifetime)` 提供完整寿命；
仅用 create_owned 返回裸指针并借用绑定，仍不足以让异步快照越过 manager 的生命周期。
来源/allocator/模块的清理约束与 [IO 请求协议](io.md#请求取消与错误)一致，资源缓存本身不会因 remount 自动替换。

目前每个新操作复制 provider 的全部 GUID 绑定，预加载批次仅复制一次；大规模索引的成本尚无吞吐保证。
原生与显式同步 worker 适配的完整组合见 [异步 VFS 示例](../../examples/beta/assets_async_vfs.cpp)。

## 显式刷新与默认绑定

`database`、`asset_manager` 和 `asset_group` 提供 `request_refresh<T>(guid)`。
manager/group 复用已配置的 worker/completion；低层 `request_refresh_into` 可使用请求表。
刷新不要求已有缓存，总是捕获新的来源快照并独立启动，不加入旧 load/reload；仅作用于单资源，不刷新依赖闭包。
loader 必须使用 context.snapshot_as<resources::provider_snapshot>() 才获得来源冻结保证；旧 JSON 同步 adapter 或忽略该字段的自定义 loader
仍按原实时 provider 行为读取，refresh 无法强制改写其内部来源选择。

| 调用 | 已有缓存 | 无缓存且最新刷新仍在途 | 旧操作仍在途 |
| --- | --- | --- | --- |
| request_load | 立即返回已发布缓存 | 订阅当前刷新 | 没有刷新选择时保留原共享规则 |
| request_reload | 无当前刷新时保留原重载规则 | 无缓存仍报 not_found | 有当前刷新时订阅它，否则保留原共享规则 |
| request_refresh | 独立准备新对象 | 独立创建更新的刷新意图 | 不加入旧操作 |

最新登记的刷新意图拥有默认绑定发布资格；首次 load 和旧 reload 的提交也必须检查该资格。
旧请求仍可返回自己加载的拥有式 handle，但不能更新默认 cache。
成功在 completion 所属线程切换单资源绑定，失败保留旧对象；最新意图失败、操作取消、投递拒绝/丢弃，
均不恢复更早意图的资格。独立订阅取消继续遵循既有共享规则，有其他订阅时不必取消底层操作。
外部旧 handle 继续可用；它不会被新绑定修改或自动切换。

发布凭据按 cache/T/GUID 隔离，内部使用不可复用的拥有式身份，而非内容 hash 或 source_id。
每个刷新过的 GUID 在 cache 内保留一个最新凭据，erase/clear 不清除该屏障、不取消在途操作；
历史凭据只由仍存活的请求保留。内部在途弱引用在请求状态析构时清空，不额外保留其控制块。

cache 本身仍不支持无协调的并发访问。发起请求、查询 cache 和执行提交必须在同一所属线程，
或由宿主统一串行化；不能把发布资格保护理解为整个 cache 变成线程安全。
allocator、GUID 访问器或哈希回调不得在容器更新期间重入修改同一 cache；完成通知在更新完成后可以发起新请求。
直接同步修改 cache、运行时配置替换和依赖批次事务需宿主协调。
来源快照固定路由，不保证依赖缓存或磁盘内容属于同一修订。

同一次 provider 快照捕获只调用每个 source 的 freeze_routes 一次，GUID 位置仍逐项复制。
临时 source 映射在捕获返回时销毁，下一次捕获重新查询来源；不按 provider generation 跨次复用，
因此 rebind/unbind 或嵌套 remount 不会因本优化被遗漏。捕获期间仍禁止并发修改 provider/source 配置。
该优化没有把全量 GUID 捕获变成 O(1)，额外临时索引为 O(不同 source 数)。
