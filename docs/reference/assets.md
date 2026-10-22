<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Assets

`assets` 是建立在 `resources` 之上的工程资源入口。它可以依赖 `resources`、`serialization`、`io`
和运行期配置，用来把可反序列化配置转换为一组可工作的 source、manifest、provider、cache、loader
和 database 绑定。

底层资源身份、缓存、provider、loader/saver、异步请求和 reload 语义见 [resources.md](resources.md)。

## 模块边界

- `resources`：底层资源核心，不绑定工程配置和具体格式。
- `assets`：工程配置和运行期资源环境构建层，包含 `project_config`、`asset_manager`、source/type
  factory 和基于 reader/writer 的 `load_serialized()` / `save_serialized()` helper。
- `assets/config_schema.h`：typed factory options 读取/诊断 helper 和 `factory_options_view`。
- `assets::json`：JSON 格式适配层，包含 JSON manifest / project config 读入、`json::loader<T>`、
  `json::saver<T>`、project config 写出和 JSON type factory 薄包装。

推荐 include：

- 只使用资源核心：`#include <caors/resources.h>`
- 使用工程配置和 asset manager：`#include <caors/assets.h>`
- 使用 JSON loader/saver 或 JSON 配置读取：`#include <caors/assets/json.h>`
- 只需要资源引用序列化 resolver/collector：`#include <caors/resources/serialization.h>`

## project_config

`assets::project_config` 是工程资源系统的可反序列化描述对象。它只保存数据，不拥有运行期
`file_system`、`source`、`provider`、`cache`、`loader` 或 `database`：

```cpp
caors::assets::project_config config;
caors::assets::json::load_project_config(config, R"({
  "schema_version": 1,
  "file_systems": [
    {
      "name": "memory",
      "type": "memory_file_system"
    },
    {
      "name": "vfs",
      "type": "resolver",
      "routes": [
        {
          "prefix": "caors://assets",
          "target": "memory"
        }
      ]
    }
  ],
  "sources": [
    {
      "name": "assets",
      "type": "file_system",
      "file_system": "vfs"
    }
  ],
  "resource_types": [
    {
      "type": "game.texture",
      "loader": "json",
      "cache": "memory",
      "options": {
        "format": "resource-json"
      }
    }
  ],
  "resources": [
    {
      "guid": "6cd2da3d-b549-4756-8b8b-0c84293af410",
      "source": "assets",
      "location": "caors://assets/textures/albedo.json",
      "type": "game.texture",
      "hash": "content-hash",
      "version": 1
    }
  ],
  "dependencies": [
    {
      "resource": "0c2f82f8-2b3e-4f77-965c-4f2d1c4a907a",
      "dependency": "6cd2da3d-b549-4756-8b8b-0c84293af410"
    }
  ],
  "preload": [
    "0c2f82f8-2b3e-4f77-965c-4f2d1c4a907a"
  ]
})");
```

当前语义：

- `schema_version` 缺省为 1；当前只接受版本 1。
- `file_systems[]` 描述运行期文件系统名称、内置类型和 routes。内置类型当前包括
  `memory_file_system` / `memory`、`native_file_system` / `native`、`resolver` /
  `file_system_resolver`、`mount` / `mount_file_system`。
- `file_systems[].routes[]` 在 `resolver` 中表示 URI 前缀路由，在 `mount` 中表示虚拟路径前缀挂载；
  `target` 引用另一个已配置或已绑定的 file_system，`target_prefix` 是目标内部路径前缀。
- `sources[]` 描述 source 名称、策略名、可选 root、可选 `file_system` 引用和 options。
  `type = "file_system"` 会把指定 file_system 适配为 `resources::file_source`；如果省略 `type`
  但填写了 `file_system`，读取配置时会默认使用 `"file_system"`。
- `resources[]` 描述 `guid -> source/location/type/hash/version`，会写入 `manifest` 和 `provider`。
  `location` 会按 `io::normalize_location()` 规范化，因此可以使用普通路径，也可以使用
  `caors://`、`file://`、`pack://` 等 URI。
- `resource_types[]` 描述 manifest type 到 cache/loader 策略名的映射。
- `dependencies[]` 写入内部 `dependency_graph`。
- `preload[]` 只保存建议预加载入口；调用方决定何时预加载。

配置读取推荐通过格式无关的 loader 接口接入，上层只依赖 `assets::project_config_loader`：

```cpp
caors::assets::project_config config;
caors::assets::json::project_config_loader loader;

caors::error status =
    caors::assets::load_project_config(config, loader, source, "project.caors");
```

`assets::json::project_config_loader` 只是 JSON 格式适配。后续 YAML、二进制包索引、远程工程配置或编辑器
数据库可以实现同一个 `project_config_loader` 接口，asset manager 不需要关心具体配置格式。

配置保存有两层入口：

```cpp
caors::assets::project_config config;

caors::variant dom = caors::assets::to_value(config);

caors::serialization::serialization_text_result json =
    caors::assets::json::save_project_config(config);

caors::string output;
caors::error status = caors::assets::json::save_project_config_to(config, output);

caors::resources::file_source source{file_system};
status = caors::assets::json::save_project_config_to(config, source, "project.caors");
```

`assets::to_value()` 和 `assets::write_project_config()` 是格式无关入口，JSON 只是其中一个 writer 适配。
保存会写回 `schema_version`、`file_systems`、`sources`、`resource_types`、`resources`、`dependencies`
和 `preload`，因此可以做 `project_config -> JSON -> project_config` roundtrip。运行期对象仍不会被保存，
只保存足够重建它们的配置描述。

## asset_manager

`assets::asset_manager` 是第一版拥有式运行期外壳。它持有 `manifest`、
`dependency_graph` 和内部 `resources::database`；工程 provider 位于私有装配状态中，减少手动拼装生命周期错误：

```cpp
caors::assets::asset_manager assets;
caors::assets::project_builder builder{assets};

caors::assets::json::register_type_factory<texture>(
    builder,
    caors::string_name{"game.texture"});

caors::error built = builder.build(config);
if (built) {
  // 处理配置或运行期对象创建错误。
}

caors::assets::asset_build_result detailed = builder.build_detailed(config);
if (detailed.failed()) {
  // detailed.stage / index / name / type / guid / reasons 可用于工具层诊断。
}

caors::resources::preload_result result = assets.preload(texture_id);
```

工程装配由 `assets::project_builder` 执行；它借用 manager，析构不销毁已创建对象。
当前配置对象、注册表和工厂集中在 manager 持有的私有工程状态中；公开装配入口已迁到 builder。
工程状态使用 manager 的 memory_resource 分配和释放，并新增一次状态对象分配；未测量性能收益。
这是分阶段迁移，不是完整的 VFS 解耦。
构建失败保留既有逐阶段结果，不提供事务回滚。

工程装配入口（均属于 builder）：

- `provider()`：访问工程来源定位与字节适配，manager 不再公开该入口。
- `use_provider_snapshot()`：显式恢复工程 VFS 批次捕获；活动操作未清理时返回 frozen。

- `build(config)`：按配置重建内部 manifest、provider 和 dependency graph，并按已注册 factory
  创建运行期 file_system、source、cache 和 loader。配置构建顺序是 file_system、source、
  resource type、resource、dependency。
- `build_detailed(config)`：执行同样构建流程，但返回 `asset_build_result`，用于定位失败阶段和可读原因。
- `bind_file_system()` / `find_file_system()`：手动绑定或查询外部 file_system；配置创建的
  file_system 会在下一次 `build()` 前清理，手动绑定会保留。
- `register_file_system_factory()`：把 `file_systems[].type` 策略名映射到运行期 file_system 创建逻辑。
- `register_typed_file_system_factory<Options, Create>()`：先把 `file_systems[].options`
  读入强类型配置并完成 schema 校验，再调用用户工厂。
- `bind_source()` / `find_source()`：手动绑定或查询外部 source；配置创建的 source 会在下一次
  `build()` 前清理，手动绑定会保留。
- `register_source_factory()`：把 `sources[].type` 策略名映射到运行期 source 创建逻辑。
- `register_typed_source_factory<Options, Create>()`：先把 `sources[].options`
  读入强类型配置并完成 schema 校验，再调用用户工厂。
- `register_type_factory()`：把 manifest type 映射到运行期 cache/loader 创建逻辑。
- `register_typed_type_factory<Options, Create>()`：先匹配 resource type 的 cache/loader 策略，
  再把 `resource_types[].options` 读入强类型配置并完成 schema 校验，最后调用用户工厂创建 cache/loader。
资源使用入口（属于 manager）：

- `find<T>(guid)` / `find(guid)`：查询内部已注册 cache 中的资源。
- `load<T>(guid)` / `load_handle<T>(guid)`：通过内部 database 中已注册的 loader/cache 加载资源。
- `reload<T>(guid)` / `reload_handle<T>(guid)`：重新加载并替换 cache 当前绑定；失败时保留旧资源。
- `plan_preload<T>(root)` / `preload<T>(root)`：按同一资源类型生成和执行依赖闭包预加载。
- `plan_preload(root)` / `preload(root)`：按 manifest type 跨类型分派预加载。
- `request_load<T>()` / `request_reload<T>()` / `request_preload()`：使用初始化的默认调度，也保留显式 worker/completion 重载；异步 loader 可稍后交付，cache 在完成 executor 提交。
- `create_group(worker, completion)`：创建复用固定调度组合的 asset_group，详见下节。
- `resolve(ref<T>&)` / `is_current(ref<T>)`：通过内部 cache 解析资源引用并检查 generation 是否仍当前。
- `replace_dependencies()` / `affected_by()` / `affected_closure()`：维护和查询内部依赖图。

异步入口不拥有传入的 executor，也不延长内部 database、cache、loader、provider 或 source 的生命周期。
调用方应让完成 executor 运行在拥有 cache 的线程，并在销毁 `asset_manager` 前停止提交且 drain 或取消
在途工作。需要 coroutine 等待时显式包含 `caors/resources/coroutine.h`：

```cpp
auto request = assets.request_load<texture>(texture_id, worker, main_executor);
auto result = co_await caors::resources::async_wait(request);
```

Assets 不额外提供重复的 `async_load()` 包装；启动请求与等待保持两个可独立组合的步骤。

### 默认调度与 asset_group

运行期调度配置与可序列化 project_config 分开：

```cpp
caors::assets::asset_manager assets{
    caors::assets::asset_execution_options{worker, main_executor}};
auto request = assets.request_load<texture>(texture_id);

auto group = assets.create_group(other_worker, other_completion);
auto grouped = group.request_load<texture>(texture_id);
auto reloaded = group.request_reload<texture>(texture_id);
auto batch = group.request_preload(root_id);
```

原无调度配置的构造继续可用。set_execution_options() 可在所属线程初始化/修改默认值；
实际操作未清理时返回 frozen，无效组合返回 invalid_argument。未配置默认值的便捷异步入口返回失败请求，
不隐式创建线程池；同步接口、显式传参入口和显式创建的 group 仍可使用。
manager/group 只保存 executor 句柄，不拥有线程或队列；completion 必须运行在 cache 所属线程。
修改默认调度不迁移 cache 线程，也不会修改既有 group 的固定组合。

| group 请求 | 行为 |
| --- | --- |
| request_load<T>(guid) | 获取单个资源，命中缓存直接完成，否则发起或订阅共享加载 |
| request_reload<T>(guid) | 重新读取，成功替换缓存，失败保留旧资源 |
| request_preload(root) / request_preload<T>(root) | 按已登记依赖图预加载跨类型/同类型闭包，复用缓存，不强制刷新 |

group 关联同一 manager/cache/请求表，可复制或移动；复制不复制资源，析构不取消已发请求。
manager 必须覆盖所有 group 访问及实际工作寿命，不保证 manager 销毁后 group 可访问。
同一共享操作由首次请求确定实际调度，后续 group 的订阅不会重新选择 worker/completion；
cancel() 只取消订阅，cancel_shared() 影响共享操作，group 不提供统一取消、批量等待或独立缓存。
等待方需要特定恢复线程时应显式调度，不能假设一定在本 group 的 completion 恢复。

project_builder::build/build_detailed() 在实际操作仍活跃时拒绝重配，返回 frozen。完成凭证或进度通道未释放也计入实际操作；
仅 request.done() 不足以证明安全。清空 database 类型绑定或重新 build 后，旧 group 返回 frozen，须重新创建；
只修改默认调度不会使 group 失效。宿主仍须在 manager 析构前清理 I/O、队列、结果内存与模块 pin。
原生异步协议、错误及取消细节以[资源 loader 参考](resources.md#原生异步-loader)为准。


需要把 `asset_manager` 挂到 `serialization::context` 解析 `resource_ref<T>` 时，使用 adapter 自由函数：

```cpp
caors::resources::resource_ref_resolver_ops resolver =
    caors::assets::make_resource_resolver(assets);

caors::serialization::context context{};
context.resolvers.external_refs.resolver = &resolver;
context.resolvers.external_refs.resolution =
    caors::resources::resource_ref_resolution::resolve_if_present;
```

## Factory 扩展

外部扩展可以注册自己的 file_system/source/type factory。factory 会收到 context 和可选 `user_data`。
工厂上下文通过 `context.project()` 取得借用装配入口；它不会延长 manager 的寿命。
`create_owned`、文件系统/source 绑定和配置工厂注册不再是 manager 的公共接口。

公开扩展类型统一命名为 `project_builder::file_system_factory`、`project_builder::source_factory` 和
`project_builder::type_factory`；底层 ops 命名只用于模块内部实现，不作为用户侧 API 约定。

file_system factory 用来把 `file_systems[]` 配置转换成运行期 `io::file_system`：

```cpp
struct editor_file_system_state {
  editor_server* server = nullptr;
};

caors::error make_editor_file_system(
    caors::assets::project_builder::file_system_factory_context& context,
    const caors::assets::project_file_system_config& config,
    void* user_data) {
  auto* state = static_cast<editor_file_system_state*>(user_data);
  auto* file_system =
      context.project().create_owned<editor_file_system>(state->server, config.root);
  if (file_system == nullptr) {
    return {caors::error_domain::resources, caors::errc::out_of_memory};
  }

  return context.project().bind_file_system(config.name, *file_system);
}

editor_file_system_state fs_state{&server};
builder.register_file_system_factory(
    caors::string_name{"editor_file_system"},
    caors::assets::project_builder::file_system_factory{&make_editor_file_system, &fs_state});
```

source factory 用来把 `sources[]` 配置转换成运行期 `resources::source`：

```cpp
struct pak_state {
  pak_index* index = nullptr;
};

caors::error make_pak_source(caors::assets::project_builder::source_factory_context& context,
                             const caors::assets::project_source_config& config,
                             void* user_data) {
  auto* state = static_cast<pak_state*>(user_data);
  caors::assets::factory_options_view options{config.options};
  std::string_view compression;
  (void)options.read(caors::string_name{"compression"}, compression);

  auto* file_system = context.project().create_owned<pak_file_system>(config.root, state->index);
  auto* source = context.project().create_owned<caors::resources::file_source>(*file_system);
  return context.project().bind_source(config.name, *source);
}

pak_state state{&index};
builder.register_source_factory(
    caors::string_name{"pak"},
    caors::assets::project_builder::source_factory{&make_pak_source, &state});
```

类型工厂同样可以创建 cache/loader，并把它们注册到内部 database：

```cpp
caors::error make_texture_runtime(
    caors::assets::project_builder::type_factory_context& context,
    const caors::assets::project_resource_type_config& config,
    void* user_data) {
  (void)user_data;
  auto* cache = context.project().create_owned<caors::resources::cache<texture>>(context.resource);
  auto* loader = context.project().create_owned<pak_texture_loader>(context.provider);
  return context.database.register_type(*cache, *loader, config.type);
}

builder.register_type_factory(
    caors::string_name{"game.texture"},
    caors::assets::project_builder::type_factory{&make_texture_runtime});
```

如果 loader 只需要 `resources::provider` 就能构造，可以复用通用 cached type factory：

```cpp
caors::assets::register_cached_type_factory<texture, pak_texture_loader>(
    assets,
    caors::string_name{"game.texture"},
    caors::string_name{"memory"},
    caors::string_name{"pak"});
```

factory 实现读取配置时优先使用 `factory_options_view`，避免直接重复写 `find_member_variant()` 和
`try_as_*()`：

```cpp
caors::assets::factory_options_view options{config.options};

std::string format;
std::uint64_t priority = 0;
bool readonly = false;

(void)options.read(caors::string_name{"format"}, format);
(void)options.read(caors::string_name{"priority"}, priority);
(void)options.read(caors::string_name{"read_only"}, readonly);
```

raw factory 不自动校验 `options`，适合极薄适配、实验性入口或完全自定义解析逻辑。如果配置需要默认值、
必填、非空、范围、允许值或未知字段拒绝，推荐注册为 typed factory：先定义强类型配置结构体，
再用反射属性 schema 描述默认值和约束。
`asset_manager` 会在调用用户 factory 前读取、应用默认值、拒绝未知字段并执行 `validate_config()`，
失败原因会进入 `build_detailed().reasons`，用户 factory 只接收已校验的 options：

```cpp
struct texture_options {
  std::string format;
  int priority = 0;
};

caors::registration::type_<texture_options>("game.texture_options")
    .meta()
    .property("format", &texture_options::format,
              caors::meta::registration::required(),
              caors::meta::registration::non_empty(),
              caors::meta::registration::one_of({"json", "binary"}),
              caors::meta::registration::default_value("json"))
    .property("priority", &texture_options::priority,
              caors::meta::registration::range(1, 100),
              caors::meta::registration::default_value(1))
    .serialization()
    .commit();

caors::error make_texture_source(
    caors::assets::project_builder::source_factory_context& context,
    const caors::assets::project_source_config& config,
    const texture_options& options,
    void* user_data) {
  (void)user_data;
  // options 已经完成默认值、DOM 覆盖和 validate_config()。
  auto* file_system = context.project().create_owned<pak_file_system>(config.root, options.format);
  auto* source = context.project().create_owned<caors::resources::file_source>(*file_system);
  return context.project().bind_source(config.name, *source);
}

builder.register_typed_source_factory<texture_options, &make_texture_source>(
    caors::string_name{"pak"});
```

`register_typed_file_system_factory`、`register_typed_source_factory` 和
`register_typed_type_factory` 都使用同一规则。`read_typed_factory_options<T>()` 仍保留为高级 helper，
适合需要在自定义流程里手动读 options 的场景。它默认拒绝未知字段，避免配置拼写错误被静默忽略；需要兼容旧配置时可传入
`unknown_field_policy::ignore` 或配合序列化 schema migration 先改写 DOM。

typed factory options 按“先应用默认值、再用输入局部覆盖”的配置语义处理缺失字段；普通对象反序列化仍默认要求
输入包含完整字段。

## JSON 适配

`assets::json` 提供 JSON 格式的薄适配：

- `assets::json::load_manifest()`：从 JSON DOM、文本或 source 读入资源 manifest。
- `assets::json::load_project_config()`：从 JSON DOM、文本或 source 读入 `project_config`。
- `assets::json::save_project_config()`：把 `project_config` 保存为 JSON 字符串。
- `assets::json::save_project_config_to()`：把 `project_config` 写入调用方提供的字符串或 source。
- `assets::json::project_config_loader`：把 JSON 配置适配到格式无关 `assets::project_config_loader` 接口。
- `assets::json::loader<T>`：从 provider 打开 JSON 输入流，再通过 `assets::load_serialized<T>()`
  构造资源。
- `assets::json::saver<T>`：从 provider 打开 JSON 输出流，把资源对象属性序列化为 JSON 文本后显式
  `close()` 提交。
- `assets::json::register_type_factory<T>()`：复用
  `assets::register_cached_type_factory<T, assets::json::loader<T>>()`，固定匹配
  `cache = "memory"`、`loader = "json"`。

示例：

```cpp
caors::assets::json::loader<texture> loader{provider};
caors::resource_cache<texture> cache;

auto loaded = caors::resources::load_handle_into(cache, loader, texture_id);

caors::assets::json::saver<texture> saver{provider};
caors::error saved = saver.save(**loaded);
```

需要注意：

- `load_serialized<T>()` 要求 `T` 派生自 `resources::resource` 且可默认构造。
- 读写的资源类型仍需要完成 meta 属性和 serialization 注册。
- helper 只读写资源对象属性；资源 GUID 由外部资源数据库、路径或包索引维护，不写入资源对象本体。
- `json::loader<T>` 和 `json::saver<T>` 不拥有 provider；provider、source 和底层 file_system 的生命周期
  由调用方保证。
- `json::saver<T>` 要求资源携带非 nil GUID，且 provider 中已有对应 GUID 绑定；绑定到只读 source 时会返回
  `errc::unsupported`。

## VFS / Source / Loader 关系

当前资源读取链路按职责分层：

- `io::file_system`：只负责路径或 URI 到字节流的访问。`memory_file_system`、`native_file_system`、
  `mount_file_system`、`file_system_resolver` 都属于这一层。
- `resources::source`：把 file_system 或其他字节来源适配给资源层，提供 `open_read()` /
  `open_write()`。一个 source 可以指向 VFS，也可以指向普通 native/memory file_system。
- `resources::provider`：保存 `guid -> source + location` 绑定。它不解析格式，也不持有资源对象。
- `resources::loader<T>`：从 provider 打开输入流，并把字节反序列化为 `T`。
- `resources::cache<T>`：按 GUID 持有已经加载完成的 `T`。
- `resources::database` / `assets::asset_manager`：把 manifest、provider、loader 和 cache 拼起来，提供
  `load()`、`reload()`、`preload()` 和异步请求入口。

因此 VFS 不替代 source；VFS 解决“从哪里打开字节”，source 解决“资源层如何统一打开字节”。配置中
`file_systems[]` 先描述可访问的文件系统，再由 `sources[]` 选择其中一个 file_system 形成资源来源。

高层 assets 配置、JSON loader/saver 和 asset manager 测试位于 `tests/assets/`。
