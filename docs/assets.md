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
- `assets::json`：JSON 格式适配层，包含 JSON manifest / project config 读入、`json::loader<T>`、
  `json::saver<T>` 和 JSON type factory 薄包装。

推荐 include：

- 只使用资源核心：`#include <caors/resources.h>`
- 使用工程配置和 asset manager：`#include <caors/assets.h>`
- 使用 JSON loader/saver 或 JSON 配置读取：`#include <caors/assets/json.h>`
- 只需要资源引用序列化 resolver/collector：`#include <caors/resources/serialization.h>`

## project_config

`assets::project_config` 是工程资源系统的可反序列化描述对象。它只保存数据，不拥有运行期
`source`、`provider`、`cache`、`loader` 或 `database`：

```cpp
caors::assets::project_config config;
caors::assets::json::load_project_config(config, R"({
  "schema_version": 1,
  "sources": [
    {
      "name": "assets",
      "type": "memory_file_system",
      "root": "/",
      "options": {
        "mount": "debug-assets"
      }
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
      "location": "textures/albedo.json",
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
- `sources[]` 描述 source 名称、策略名、可选 root 和 options。
- `resources[]` 描述 `guid -> source/location/type/hash/version`，会写入 `manifest` 和 `provider`。
- `resource_types[]` 描述 manifest type 到 cache/loader 策略名的映射。
- `dependencies[]` 写入内部 `dependency_graph`。
- `preload[]` 只保存建议预加载入口；调用方决定何时预加载。

## asset_manager

`assets::asset_manager` 是第一版拥有式运行期外壳。它持有 `manifest`、`provider`、
`dependency_graph` 和内部 `resources::database`，减少手动拼装生命周期错误：

```cpp
caors::assets::asset_manager assets;

caors::assets::json::register_type_factory<texture>(
    assets,
    caors::string_name{"game.texture"});

caors::error built = assets.build(config);
if (built) {
  // 处理配置或运行期对象创建错误。
}

caors::assets::asset_build_result detailed = assets.build_detailed(config);
if (detailed.failed()) {
  // detailed.stage / index / name / type / guid / reasons 可用于工具层诊断。
}

caors::resources::preload_result result = assets.preload(texture_id);
```

常用入口：

- `build(config)`：按配置重建内部 manifest、provider 和 dependency graph，并按已注册 factory
  创建运行期 source、cache 和 loader。
- `build_detailed(config)`：执行同样构建流程，但返回 `asset_build_result`，用于定位失败阶段和可读原因。
- `find<T>(guid)` / `find(guid)`：查询内部已注册 cache 中的资源。
- `load<T>(guid)` / `load_handle<T>(guid)`：通过内部 database 中已注册的 loader/cache 加载资源。
- `reload<T>(guid)` / `reload_handle<T>(guid)`：重新加载并替换 cache 当前绑定；失败时保留旧资源。
- `plan_preload<T>(root)` / `preload<T>(root)`：按同一资源类型生成和执行依赖闭包预加载。
- `plan_preload(root)` / `preload(root)`：按 manifest type 跨类型分派预加载。
- `request_load<T>()` / `request_reload<T>()` / `request_preload()`：把资源加载提交到 executor，并在
  completion queue 中完成 cache commit。
- `resolve(ref<T>&)` / `is_current(ref<T>)`：通过内部 cache 解析资源引用并检查 generation 是否仍当前。
- `replace_dependencies()` / `affected_by()` / `affected_closure()`：维护和查询内部依赖图。

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

外部扩展可以注册自己的 source/type factory。factory 会收到 context 和可选 `user_data`：

```cpp
struct pak_state {
  pak_index* index = nullptr;
};

caors::error make_pak_source(caors::assets::asset_manager::source_factory_context& context,
                             const caors::assets::project_source_config& config,
                             void* user_data) {
  auto* state = static_cast<pak_state*>(user_data);
  caors::assets::factory_options_view options{config.options};
  std::string_view compression;
  (void)options.read(caors::string_name{"compression"}, compression);

  auto* file_system = context.manager.create_owned<pak_file_system>(config.root, state->index);
  auto* source = context.manager.create_owned<caors::resources::file_source>(*file_system);
  return context.manager.bind_source(config.name, *source);
}

pak_state state{&index};
caors::assets::factory_option_schema pak_schema;
pak_schema.optional(caors::string_name{"compression"},
                    caors::assets::factory_option_kind::string);
pak_schema.allow_unknown = false;

assets.register_source_factory(caors::string_name{"pak"}, &make_pak_source, &state, pak_schema);
```

类型工厂同样可以创建 cache/loader，并把它们注册到内部 database：

```cpp
caors::error make_texture_runtime(
    caors::assets::asset_manager::type_factory_context& context,
    const caors::assets::project_resource_type_config& config,
    void* user_data) {
  (void)user_data;
  auto* cache = context.manager.create_owned<caors::resources::cache<texture>>(context.resource);
  auto* loader = context.manager.create_owned<pak_texture_loader>(context.provider);
  return context.database.register_type(*cache, *loader, config.type);
}

caors::assets::factory_option_schema texture_schema;
texture_schema.require(caors::string_name{"format"},
                       caors::assets::factory_option_kind::string)
    .non_empty(caors::string_name{"format"})
    .one_of(caors::string_name{"format"},
            {std::string_view{"json"}, std::string_view{"binary"}});
texture_schema.optional(caors::string_name{"priority"})
    .range(caors::string_name{"priority"}, 1.0, 100.0);

assets.register_type_factory(caors::string_name{"game.texture"}, &make_texture_runtime, nullptr,
                             texture_schema);
```

如果 loader 只需要 `resources::provider` 就能构造，可以复用通用 cached type factory：

```cpp
caors::assets::register_cached_type_factory<texture, pak_texture_loader>(
    assets,
    caors::string_name{"game.texture"},
    caors::string_name{"memory"},
    caors::string_name{"pak"});
```

`factory_option_schema` 只做轻量配置校验：必填字段、字段值形态、非空约束、数值范围、字符串允许值和
未知字段策略。`build_detailed()` 会把同一个 options 对象中的多个 schema 错误收集到
`asset_build_result::reasons`，方便工具和编辑器一次展示多条配置问题。

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

复杂配置推荐先反序列化到 typed config，再用反射字段 metadata 做校验。这样可以复用序列化系统，
并把工厂内部的配置读取收敛到一个结构体上：

```cpp
struct texture_options {
  std::string format;
  int priority = 1;
};

namespace metadata = caors::serialization::config_metadata;

caors::meta::class_<texture_options>("game.texture_options");
caors::meta::class_<texture_options>()
    .field("format", &texture_options::format)
    .field_metadata(metadata::required(), true)
    .field_metadata(metadata::non_empty(), true)
    .field_metadata(metadata::one_of(),
                    caors::meta::make_metadata_string_array({"json", "binary"}))
    .field("priority", &texture_options::priority)
    .field_metadata(metadata::range_min(), 1)
    .field_metadata(metadata::range_max(), 100)
    .commit();
caors::serialization::class_<texture_options>().commit();

texture_options options;
caors::error read_status = caors::deserialize(options, config.options);
caors::serialization::config_validation_result validation =
    caors::serialization::validate_config(options);
```

`factory_option_schema` 仍适合简单、无需结构体绑定的插件入口；typed config + metadata 更适合
复杂配置、工具面板和后续 schema metadata 扩展。

## JSON 适配

`assets::json` 提供 JSON 格式的薄适配：

- `assets::json::load_manifest()`：从 JSON DOM、文本或 source 读入资源 manifest。
- `assets::json::load_project_config()`：从 JSON DOM、文本或 source 读入 `project_config`。
- `assets::json::loader<T>`：从 provider 读取 JSON 字节，再通过 `assets::load_serialized<T>()`
  构造资源。
- `assets::json::saver<T>`：把资源对象字段序列化为 JSON 文本，再写回 provider。
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
- 读写的资源类型仍需要完成 meta 字段和 serialization 注册。
- helper 只读写资源对象字段；资源 GUID 由外部资源数据库、路径或包索引维护，不写入资源对象本体。
- `json::loader<T>` 和 `json::saver<T>` 不拥有 provider；provider、source 和底层 file_system 的生命周期
  由调用方保证。
- `json::saver<T>` 要求资源携带非 nil GUID，且 provider 中已有对应 GUID 绑定；绑定到只读 source 时会返回
  `errc::unsupported`。

高层 assets 配置、JSON loader/saver 和 asset manager 测试位于 `tests/assets/`。
