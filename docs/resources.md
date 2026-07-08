<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Resources

本文档说明 CAORS 资源系统的当前设计边界。资源系统仍处于基础阶段，当前提供资源身份、轻量引用模型、
运行期共享 handle、非拥有资源索引、引用计数缓存、资源依赖图、资源清单、字节来源 provider、
加载/保存接口、非拥有资源 database 编排入口、同步 reload、显式 reload 通知、最小序列化读写 helper、
同步预加载计划、序列化资源引用 resolver 和第一版异步加载请求；资源包后续再补。

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
- 资源加载器和保存器先只定义类型化接口，不绑定路径、文件系统或异步框架。
- 资源 database 可以把 manifest、provider、cache、loader、依赖图和序列化 resolver 串成一条主流程。
- 同步预加载计划可以按依赖图计算依赖优先的加载顺序，并在需要拓扑顺序的流程中检测循环依赖。
- 异步加载请求可以把后台 loader 阶段和主线程 cache commit 阶段分开。
- 底层资源模型不绑定具体引擎、文件系统、异步加载框架或编辑器管线。

当前不会实现：

- 资源包或网络加载后端。
- 拥有式全局资源缓存。
- cache 级自动淘汰、后台释放或跨 cache 生命周期协调。
- 完整热重载系统。
- 资源导入管线。
- 跨资源类型自动预加载分派。
- 请求优先级、进度聚合和同 GUID 请求合并。
- 与对象系统的场景树生命周期绑定。

这些能力后续应建立在 `io::file_system`、`resources::provider`、`resources::manager`、
`resources::loader` / `resources::saver` 和上层资源数据库之上。

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
- `resources::source`
- `resources::file_source`
- `resources::provider`
- `resources::loader<T>`
- `resources::json_loader<T>`
- `resources::saver<T>`
- `resources::json_saver<T>`
- `resources::request_status`
- `resources::load_request<T>`
- `resources::preload_plan`
- `resources::preload_result`
- `resources::request_load_into()`
- `resources::request_reload_into()`
- `resources::reload_event<T>`
- `resources::reload_signal<T>`

根命名空间保留常用别名：

- `caors::resource_guid`
- `caors::resource_guid_hash`
- `caors::resource`
- `caors::resource_ref<T>`
- `caors::resource_handle<T>`
- `caors::resource_weak_handle<T>`
- `caors::resource_manager`
- `caors::resource_cache<T>`
- `caors::resource_dependency_graph`
- `caors::resource_database`
- `caors::resource_manifest`
- `caors::resource_manifest_entry`
- `caors::resource_source`
- `caors::resource_file_source`
- `caors::resource_provider`
- `caors::resource_load_result<T>`
- `caors::resource_handle_result<T>`
- `caors::resource_loader<T>`
- `caors::resource_json_loader<T>`
- `caors::resource_saver<T>`
- `caors::resource_json_saver<T>`
- `caors::resource_request_status`
- `caors::resource_load_request<T>`
- `caors::resource_preload_plan`
- `caors::resource_preload_result`
- `caors::resource_reload_event<T>`
- `caors::resource_reload_signal<T>`
- `caors::bind_manifest()`
- `caors::adopt_handle()`
- `caors::load_into()`
- `caors::load_handle_into()`
- `caors::load_manifest()`
- `caors::load_manifest_json()`
- `caors::load_serialized()`
- `caors::make_handle()`
- `caors::replace_manifest()`
- `caors::reload_and_notify()`
- `caors::reload_handle_into()`
- `caors::request_load_into()`
- `caors::request_reload_into()`
- `caors::save_from()`
- `caors::save_serialized()`
- `caors::sync_manifest()`

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
- `resource` 和 `objects::object` 的边界不同：资源表示可持久化、可缓存、可复用资产；对象表示当前运行期实例。

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

`resources::database` 提供第一版同步预加载入口：

```cpp
caors::resource_preload_plan plan =
    database.plan_preload<texture>(texture_id);

if (plan.succeeded()) {
  caors::resource_preload_result result =
      database.preload<texture>(texture_id);
}
```

当前语义：

- `plan_preload<T>(root)` 从 `root` 出发遍历依赖闭包，生成依赖优先的 `load_order`。
- 已经在已注册 `cache<T>` 中的资源会进入 `already_loaded`，不会重复加载。
- 如果 database 设置了 manifest，缺失清单条目会进入 `missing_manifest`，计划返回
  `resources / not_found`。
- 如果依赖闭包中存在环，`cycle` 保存一个闭合路径，计划返回 `resources / cycle_detected`。
- `preload<T>(root)` 会先生成计划，再按 `load_order` 调用已注册 `loader<T>` 和 `cache<T>`。

当前限制：

- 第一版假定依赖闭包内资源都由同一个 `T` 类型的 cache/loader 处理。
- 不会根据 manifest `type`、`type_guid` 或外部资源数据库自动选择不同资源类型。
- 不会自动递归解析对象、场景或资源包层级的依赖；这些能力后续建立在当前计划结构之上。

## 异步加载请求

第一版异步资源 API 只拆分两个阶段：

- 后台阶段：通过 `tasks::executor` 调用 `loader<T>::load()`，读取 provider、解码数据并构造资源对象。
- 主线程阶段：通过 `tasks::completion_queue` 把加载结果提交到 `cache<T>`，执行 insert 或 replace。

直接使用 cache 和 loader：

```cpp
caors::resource_cache<texture> cache;
caors::resource_json_loader<texture> loader{provider};
caors::tasks::thread_pool_executor executor{2};
caors::tasks::completion_queue completions;

auto request = caors::request_load_into(cache, loader, texture_id, executor, completions);

// 后台任务完成后，在主线程明确时机提交 cache 修改。
completions.drain();

if (request.loaded()) {
  caors::resource_handle<texture> texture = request.get();
}

auto result = request.result();
if (result.failed()) {
  caors::error status = result.status;
}
```

通过 database 使用已注册 cache/loader：

```cpp
caors::resource_database database;
database.register_type(cache, loader);

auto request = database.request_load<texture>(texture_id, executor, completions);
```

重载使用 `request_reload_into()` 或 `database.request_reload<T>()`。重载成功时 cache 指向新资源；旧
`resource_handle<T>` 仍继续持有旧资源。重载失败时旧资源保持不变。

`load_request<T>` 状态：

- `queued`：请求已创建，但后台任务尚未开始。
- `loading`：后台任务正在加载，或加载完成后等待主线程 commit。
- `loaded`：主线程 commit 成功，`get()` 返回结果 handle。
- `failed`：请求失败，`error()` 返回失败原因。
- `cancelled`：请求已取消。已经开始的后台 loader 不会被强制中断，但后续 completion queue commit 会跳过，
  不再写入 cache。

常用查询：

- `cancel()`：queued/loading 状态可取消；loaded/failed/cancelled 状态返回 false。
- `done()`：loaded、failed 或 cancelled 时返回 true。
- `get()`：非阻塞；loaded 时返回结果 handle，未完成、失败或取消时返回空 handle。
- `result()`：非阻塞；loaded 返回成功和结果 handle，queued/loading 返回 `resources / pending`，
  cancelled 返回 `resources / cancelled`，failed 返回原始错误。

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

线程边界：

- `cache<T>`、`manager`、manifest、对象状态和运行期场景状态只能在主线程 commit 阶段修改。
- 后台任务可以调用 loader，但 loader、provider、source 和 memory_resource 是否能跨线程使用由调用方保证。
- `load_request<T>` 自身状态查询是线程安全的。
- `completion_queue::drain()` 应在拥有 cache 的线程调用。
- 第一版 `load_request<T>::get()` 和 `result()` 都不阻塞；等待、批量轮询和进度聚合后续再补。

## 序列化

`caors/serialization.h` 包含 `resource_guid` 和 `resource_ref<T>` 的 codec。资源引用序列化只保存稳定
GUID，不展开资源对象，也不保存运行期资源指针：

```cpp
struct material {
  caors::resource_ref<texture> albedo;
};

caors::meta::type_<caors::resource_ref<texture>>("game.resource_ref<game.texture>");
caors::serialization::register_codec<caors::resource_ref<texture>>();
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
caors::deserialize_json(decoded, R"({"albedo":"6cd2da3d-b549-4756-8b8b-0c84293af410"})");

caors::resource_cache<texture> cache;
// cache 创建或加载同 GUID 的资源后：
cache.resolve(decoded.albedo);
```

也可以通过 `serialization::context` 为本次反序列化挂一个资源 resolver。resolver 可以从
`resources::manager` 或 `resources::cache<T>` 创建：

```cpp
caors::resource_cache<texture> cache;
texture* loaded = cache.create(texture_id);

caors::serialization::resource_resolver resolver =
    caors::serialization::make_resource_resolver(cache);

caors::serialization::context context{};
context.resources.resolver = &resolver;
context.resources.resolution =
    caors::serialization::resource_ref_resolution::resolve_if_present;

material decoded{};
caors::deserialize_json(context, decoded, R"({"albedo":"6cd2da3d-b549-4756-8b8b-0c84293af410"})");
```

`context.resources.resolution` 的策略：

- `keep_unresolved`：默认策略，只恢复 GUID，不解析资源。
- `resolve_if_present`：如果 resolver 能找到同 GUID 且类型兼容的资源，则写入已解析指针；缺资源仍保持 unresolved。
- `require_resolved`：必须解析成功；resolver 缺失返回 `serialization / invalid_argument`，资源缺失返回 `resources / not_found`，类型不匹配返回 `resources / invalid_type`。

序列化和反序列化遍历 `resource_ref<T>` 时还可以收集资源依赖：

```cpp
caors::serialization::resource_dependency_list dependencies;
caors::serialization::resource_dependency_collector collector = dependencies.collector();

caors::serialization::context context{};
context.resources.dependencies = &collector;

caors::serialize_json(context, material_value);
```

依赖条目包含 `resource_guid` 和资源声明类型的 `type_id`。这可用于后续预加载、打包、热更新和资源数据库。
依赖收集器本身不加载资源，也不改变引用的 resolved 状态。常见同步使用方式是：先从对象或存档中收集
`resource_ref<T>` 依赖，再按 GUID 查询 manifest / provider，使用匹配的 loader 预加载到 `cache<T>`，
最后把 cache 适配成 `serialization::resource_resolver`，让反序列化阶段按策略解析引用。

## manager

`resources::manager` 是非拥有资源索引。它保存 `resource_guid -> resource*` 映射，可以绑定、查找、
解绑和清空资源，也可以把 `resource_ref<T>` 解析成实际资源指针：

```cpp
caors::resource_manager manager;

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

- `cache<T>` 内部保存强 handle。`create_handle()`、`find_handle()` 和 `release()` 返回强 handle；
  `create()`、`find()` 和 `load_into()` 只是返回非拥有裸指针视图。
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
- `serialization::replace_resource_dependencies(graph, resource, dependencies)` 可以把一次序列化遍历收集到的
  `resource_dependency_list` 写入依赖图。依赖条目中的 `type_id` 仍保留给 manifest、资源数据库或调试诊断使用；
  `dependency_graph` 当前只保存 GUID 关系。
- 当前第一版只建模资源到资源的直接依赖。对象、场景、资源包和编辑器导入任务可以在上层资源数据库中继续扩展。

## database

`resources::database` 是非拥有的资源编排入口。它不会创建全局单例，也不会拥有 manifest、provider、
cache、loader 或 dependency graph；调用方把这些组件挂进去后，database 只负责把常见同步流程串起来：

```cpp
caors::resource_manifest manifest;
caors::resource_provider provider;
caors::resource_dependency_graph graph;

caors::resource_database database;
database.set_manifest(manifest);
database.set_provider(provider);
database.set_dependency_graph(graph);

database.sync_manifest(source);

caors::resource_cache<texture> cache;
caors::resource_json_loader<texture> loader{provider};
database.register_type(cache, loader);

auto loaded = database.load<texture>(texture_id);
auto resolver = database.resolver();
```

常用入口：

- `set_manifest()` / `set_provider()` / `set_dependency_graph()`：挂接外部组件，不接管生命周期。
- `bind_manifest(source)` / `sync_manifest(source)` / `replace_manifest(source)`：把当前 manifest 同步到当前
  provider。
- `register_cache<T>()` / `register_loader<T>()` / `register_type<T>()`：按已注册 meta 类型挂接类型化
  cache 和 loader。
- `load<T>(guid)` / `load_handle<T>(guid)`：通过已注册 loader 加载资源并转入对应 cache。
- `reload<T>(guid)` / `reload_handle<T>(guid)`：重新加载并替换对应 cache 中已有资源；失败时仍保留旧资源。
- `find<T>(guid)` / `find(guid)`：查询已注册 cache 中的资源。
- `resolve(ref<T>&)` / `is_current(ref<T>)`：通过已注册 cache 解析资源引用并检查 generation 是否仍当前。
- `resolver()`：生成 `serialization::resource_resolver`，可挂到 `serialization::context` 中解析
  `resource_ref<T>`。
- `replace_dependencies()` / `affected_by()` / `affected_closure()`：把依赖收集结果写入当前依赖图，并查询
  reload 影响范围。

需要注意：

- `database` 是编排器，不是拥有式全局资源缓存。资源生命周期仍由 `cache<T>` / `handle<T>` 决定。
- `database` 不会根据 manifest 的 `type` 字符串自动选择 loader；第一版仍要求调用方显式
  `register_type<T>(cache, loader)`。
- `database.resolver()` 只能返回 `resource*`；`resource_ref<T>` 的类型兼容检查仍由序列化层的
  `dynamic_cast<T*>` 完成。
- 如果希望 `resource_ref<T>` 记录 cache generation，应在读回后调用 `database.resolve(ref)` 或
  `cache.resolve(ref)`；序列化 resolver 本身只负责把 GUID 解析为资源对象。
- 跨线程修改 database、cache、provider 或 manifest 时，调用方需要自行同步。

## reload 策略现状

当前资源 reload 是一套底层同步方案，而不是完整热重载系统。它的职责划分如下：

- `resource_ref<T>` 保存稳定 `resource_guid`、可选已解析 `T*` 和运行期 generation，不拥有资源。
- `resources::cache<T>` 持有资源强 handle，并维护 GUID 到资源指针的索引。
- `resources::manifest` / `resources::provider` / `resources::loader<T>` 负责定位和重新加载资源内容。
- `resources::reload_into(cache, loader, guid)` 只负责“重新加载已有资源并替换 cache 中的对象”。
- `resources::reload_and_notify(cache, loader, guid, signal)` 在 `reload_into()` 之后同步广播
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
- 当前没有文件监听、异步加载和后台任务队列。

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
- `location`：规范化后的虚拟资源路径。
- `type`：可选资源类型名，主要用于调试、导入和上层资源数据库。
- `hash`：可选内容 hash、etag 或版本指纹。
- `version`：可选递增版本号，适合热更新和调试 fileserver 判断资源是否变化。

可以把清单批量绑定到 provider：

```cpp
caors::memory_file_system files;
caors::resource_file_source source{files};
caors::resource_provider provider;

caors::bind_manifest(provider, source, manifest);
caors::sync_manifest(provider, source, manifest);
caors::replace_manifest(provider, source, manifest);
```

也可以从 JSON 文本或 source 读取清单：

```cpp
caors::resource_manifest manifest;

caors::load_manifest_json(manifest, R"({
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

caors::load_manifest(manifest, source, "manifest.json");
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
- `bind()` 会拒绝 nil GUID、空路径和重复 GUID，并会按虚拟路径规则规范化 location。
- `set()` 会新增或替换同 GUID 条目，适合资源数据库刷新和调试 manifest 热更新。
- JSON 导入要求每个条目至少包含字符串 `guid` 和字符串 `location`；`type`、`hash`、`version` 可选。
- `load_manifest_json()` 和 `load_manifest()` 会向现有 manifest 新增或替换条目，不会隐式清空已有内容。
- `bind_manifest()` 不回滚已经成功绑定到 provider 的条目；失败时调用方需要按自身策略处理。
- `sync_manifest()` 会更新 provider 中已有 GUID 的 source/location，也会新增缺失 GUID；它不会删除清单外的旧绑定。
- `replace_manifest()` 会先清空 provider，再绑定完整清单，适合完整清单刷新；失败时不会恢复旧 provider 状态。
- 清单的 `type`、`hash`、`version` 只是轻量元数据，不替代资源对象本身的序列化内容。
- 跨线程更新 manifest 或 provider 时，调用方需要自行同步。

## source / provider

`resources::source` 是“按 `io::path_view` 位置读取字节”的接口；`resources::provider` 是
`resource_guid -> (source, location)` 的运行期索引：

```cpp
caors::native_file_system native;
caors::resource_file_source source{native};
caors::resource_provider provider;

provider.bind(texture_id, source, "assets/textures/albedo.png");

caors::byte_buffer_result bytes = provider.read(texture_id);
caors::error written = provider.write(texture_id, std::string_view{"new-bytes"});
```

`resources::file_source` 把任意 `io::file_system` 适配成资源字节来源。因此本地目录、挂载文件系统、
后续 HTTP fileserver 或资源包都可以通过同一套 provider 暴露给资源 loader。

需要注意：

- `provider` 不拥有 source，调用方需要保证 source 生命周期。
- `source_location::location` 是非拥有 `io::path_view`，provider 修改或销毁后失效。
- `provider` 只负责定位、读取和可选写入字节，不负责解码资源对象。
- `source::write()` 默认返回 `errc::unsupported`；`file_source` 会转发到后端 `io::file_system::write_all()`。
- `bind()` 会拒绝 nil GUID、空 location 和重复 GUID。
- provider 未命中 GUID 返回 `error_domain::resources / errc::not_found`。
- 具体 source 读取失败时，错误域保持来源模块语义，例如 `file_source` 可能返回 `error_domain::io`。
- 跨线程访问 provider 或 source 时，调用方需要自行同步。

## loader / saver

`resources::loader<T>` 和 `resources::saver<T>` 是类型化资源 IO 接口。它们只描述“按 GUID 加载一个
资源”和“保存一个已有资源”，不规定路径、文件格式、资源包、缓存策略或异步调度：

```cpp
class texture_loader : public caors::resource_loader<texture> {
public:
  caors::resource_load_result<texture>
  load(caors::resource_guid id, caors::memory_resource* resource) override;
};
```

加载结果使用 `resource_load_result<T>`，内部是 `result<memory::unique_ptr<T>>`。加载器返回所有权后，
可以通过 `load_handle_into()` 转入 `resources::cache<T>` 并拿到强 handle；如果只需要临时访问，
也可以使用兼容入口 `load_into()` 返回裸指针：

```cpp
caors::resource_cache<texture> cache;
texture_loader loader;

caors::resource_handle_result<texture> held =
    caors::load_handle_into(cache, loader, id);
caors::result<texture*> value = caors::load_into(cache, loader, id);
```

保存方向可以使用 `save_from()` 从 cache 取出指定 GUID 的资源并交给 saver：

```cpp
texture_saver saver;
caors::error saved = caors::save_from(cache, saver, id);
```

`load_handle_into()` / `load_into()` 的规则：

- cache 已经有同 GUID 资源时直接返回已有对象，不再次调用 loader。
- loader 返回的资源如果没有 GUID，会写入请求 GUID。
- loader 返回的资源如果携带不同 GUID，会返回 `errc::inconsistent_type`。
- 成功后资源所有权转入 cache。`load_handle_into()` 返回强 handle；`load_into()` 返回非拥有裸指针。

`reload_handle_into()` / `reload_into()` 用于同步重载 cache 中已经存在的资源：

- GUID 为 nil 时返回 `resources / invalid_argument`。
- cache 未包含该 GUID 时返回 `resources / not_found`；首次加载应继续使用 `load_into()`。
- loader 加载失败、返回空指针或返回不同 GUID 时，不修改 cache 中旧资源。
- 只有新资源完整加载且身份校验通过后，才通过 `cache.replace()` 替换旧资源。
- 替换成功后 cache 指向新资源；外部旧 `resource_handle<T>` 会继续持有旧资源，直到最后一个强引用释放。
- 外部已解析到旧资源的 `resource_ref<T>` 或裸指针不会自动置空。使用方可以先通过
  `cache.is_current(ref)` 判断引用是否过期，再重新调用 `cache.resolve(ref)`，或由上层资源系统统一处理
  引用失效通知。

如果需要同步通知，可以使用 `reload_and_notify()`：

```cpp
caors::resource_reload_signal<texture> reloaded;

auto connection = reloaded.connect(listener);
auto result = caors::reload_and_notify(cache, loader, texture_id, reloaded);
```

`reload_and_notify()` 的规则：

- 内部先调用 `reload_into()`，然后无论成功或失败都会发出一次 `resource_reload_event<T>`。
- `event.status` 表示本次 reload 结果；`event.succeeded()` 为 true 时 `event.resource` 指向 cache
  中的新资源。
- 失败时 `event.resource` 为 `nullptr`，cache 中旧资源保持不变。
- 通知是同步的，不做线程切换，也不会自动刷新外部 `resource_ref<T>`；监听者可以调用
  `cache.is_current(ref)` 和 `cache.resolve(ref)` 自行刷新。

`save_from()` 的规则：

- GUID 为 nil 时返回 `errc::invalid_argument`。
- cache 未包含该 GUID 时返回 `errc::not_found`。
- 具体保存目标、格式和覆盖策略由 saver 实现决定。

`load_serialized<T>()` / `save_serialized<T>()` 是最小序列化读写 helper，复用现有
`serialization::reader` / `serialization::writer`：

```cpp
caors::serialization::variant_reader reader{value};
auto loaded = caors::load_serialized<texture>(id, reader);

caors::serialization::json_writer writer;
caors::save_serialized(*loaded.value, writer);
```

需要传入序列化上下文时使用 context-first 形式：

```cpp
auto loaded_with_context = caors::load_serialized<texture>(context, id, reader);
caors::save_serialized(context, *loaded_with_context.value, writer);
```

`resources::json_loader<T>` 是 provider 到资源对象的第一版桥接。它按 GUID 从 provider 读取 JSON
字节，再通过 `load_serialized<T>()` 构造资源，可直接配合 `load_into()` 使用：

```cpp
caors::resource_file_source source{file_system};
caors::resource_provider provider;
provider.bind(texture_id, source, "assets/texture.json");

caors::serialization::context context{};
caors::resource_json_loader<texture> loader{provider};
caors::resource_json_loader<texture> loader_with_context{context, provider};
caors::resource_cache<texture> cache;

auto loaded = caors::load_into(cache, loader, texture_id);
```

推荐同步加载链路是：

1. `resources::manifest` 保存 `resource_guid -> location/type/hash/version`。
2. `bind_manifest()` / `sync_manifest()` / `replace_manifest()` 把 manifest 批量绑定到 provider。
3. `resources::provider` 只负责按 GUID 定位并读取字节。
4. `resources::loader<T>` 负责把字节解码为资源对象。
5. `load_into(cache, loader, guid)` 把加载结果转入拥有式 cache。
6. `serialization::resource_resolver` 从 manager/cache 查询已加载资源，用于解析 `resource_ref<T>`。

如果对象存档中包含资源引用，推荐在读回前先做一遍依赖收集和预加载：

1. 用 `serialization::resource_dependency_list` 收集所有 `resource_ref<T>` 依赖。
2. 对资源对象本身做依赖索引时，调用
   `serialization::replace_resource_dependencies(graph, current_resource_guid, dependencies)` 写入依赖图。
3. 通过 manifest 查找每个 GUID 对应的资源位置、类型和版本信息。
4. 通过 provider 读取资源字节，并用对应 loader 解码。
5. 使用 `load_into(cache, loader, guid)` 把资源放入 cache。
6. 反序列化对象时把 cache resolver 挂到 `context.resources.resolver`，按
   `resolve_if_present` 或 `require_resolved` 决定缺资源策略。

同步重载推荐流程：

1. 更新 manifest 条目的 `location`、`hash` 或 `version`。
2. 使用 `sync_manifest(provider, source, manifest)` 更新 provider 中的 GUID 到位置映射。
3. 调用 `reload_into(cache, loader, guid)` 或 `reload_and_notify(cache, loader, guid, signal)`
   重新读取 provider、解码资源并替换 cache 中旧资源。
4. 如果重载失败，`reload_into()` 保留 cache 中旧资源，调用方可以继续使用旧资源并返回诊断错误。
5. 替换成功后，可以用 `dependency_graph.affected_by(guid)` 查询直接受影响的资源，或用
   `dependency_graph.affected_closure(guid)` 查询递归受影响的资源。
6. 调用方需要重新解析相关 `resource_ref<T>`，或在上层系统里发出资源失效/重载通知。

provider 本身不直接作为 `resource_ref<T>` resolver，因为它只知道字节位置，不知道如何构造具体 `T`。
如果需要“缺失时自动加载”，应在上层实现一个自定义 resolver：内部先查询 cache，未命中时通过
provider + loader 加载，再把结果插入 cache。

`resources::json_saver<T>` 是资源对象到 provider 的同步 JSON 保存桥接。它会把资源对象字段序列化为
JSON 文本，再按资源 GUID 写回 provider 已绑定的位置：

```cpp
caors::serialization::context context{};
caors::resource_json_saver<texture> saver{provider};
caors::resource_json_saver<texture> saver_with_context{context, provider};
caors::error saved = saver.save(*loaded.value);
```

需要注意：

- `load_serialized<T>()` 要求 `T` 派生自 `resources::resource` 且可默认构造。
- 读写的资源类型仍需要完成 reflection 和 serialization 注册。
- helper 只读写资源对象字段；资源 GUID 由外部资源数据库、路径或包索引维护，不写入资源对象本体。
- `json_loader<T>` 和 `json_saver<T>` 不拥有 provider；provider、source 和底层 file_system 的生命周期
  由调用方保证。
- `json_saver<T>` 要求资源携带非 nil GUID，且 provider 中已有对应 GUID 绑定；绑定到只读 source 时会返回
  `errc::unsupported`。
- 非默认构造、需要外部依赖或需要异步调度的资源应实现自定义 `loader<T>`。

相关测试位于 `tests/resources/`，测试组织规则见 [tests/README.md](../tests/README.md)。
