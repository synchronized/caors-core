// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：resources_asset_manager_project_config_builds_runtime_resource_environment_test.cpp

#include "../../resources/resources_test_common.h"

#include "caors/tasks.h"

#include <string>
#include <string_view>

using namespace caors::tests::resources;

namespace {

constexpr caors::resource_guid test_guid_c{caors::guid::from_bytes(
    caors::guid::byte_array{0x9d, 0xe1, 0x08, 0x75, 0xa1, 0x63, 0x4b, 0x8d, 0x9c, 0x28, 0x5b, 0x28,
                            0x57, 0x49, 0x86, 0x92})};

[[nodiscard]] caors::string_name serialized_manifest_type() {
  return caors::string_name{"game.serialized"};
}

[[nodiscard]] std::string make_project_config_json() {
  std::string json{R"({
    "sources": [
      {
        "name": "assets",
        "type": "memory_file_system",
        "root": "/",
        "options": {
          "mount": "debug-assets",
          "read_only": true
        }
      }
    ],
    "resource_types": [
      {
        "type": "game.serialized",
        "loader": "json",
        "cache": "memory",
        "options": {
          "format": "resource-json",
          "priority": 2
        }
      }
    ],
    "resources": [
      {
        "guid": ")"};
  json += test_guid_a.string();
  json += R"(",
        "source": "assets",
        "location": "/assets//resource-a.json",
        "type": "game.serialized",
        "hash": "hash-a",
        "version": 1
      },
      {
        "guid": ")";
  json += test_guid_b.string();
  json += R"(",
        "source": "assets",
        "location": "assets/resource-b.json",
        "type": "game.serialized",
        "hash": "hash-b",
        "version": 2
      }
    ],
    "dependencies": [
      {
        "resource": ")";
  json += test_guid_b.string();
  json += R"(",
        "dependency": ")";
  json += test_guid_a.string();
  json += R"("
      }
    ],
    "preload": [
      ")";
  json += test_guid_b.string();
  json += R"(",
      {
        "guid": ")";
  json += test_guid_c.string();
  json += R"("
      }
    ]
  })";
  return json;
}

struct external_source_factory_state {
  int calls = 0;
  caors::memory_resource* resource = nullptr;
  caors::io::memory_file_system* file_system = nullptr;
  caors::resources::file_source* source = nullptr;
  bool read_only = false;
  std::string mount;
};

struct external_type_factory_state {
  int calls = 0;
  caors::resources::cache<serialized_resource>* cache = nullptr;
  caors::assets::json::loader<serialized_resource>* loader = nullptr;
  std::string format;
  std::uint64_t priority = 0;
};

class counted_source final : public caors::resources::source {
public:
  explicit counted_source(int* destroyed) noexcept : destroyed_(destroyed) {}
  ~counted_source() override {
    if (destroyed_ != nullptr) {
      ++*destroyed_;
    }
  }

  [[nodiscard]] bool exists(caors::io::path_view) const override { return false; }

  [[nodiscard]] caors::io::byte_buffer_result read(caors::io::path_view,
                                                   caors::memory_resource*) override {
    return {{caors::error_domain::resources, caors::errc::not_found}, {}};
  }

private:
  int* destroyed_ = nullptr;
};

struct counted_source_factory_state {
  int calls = 0;
  int destroyed = 0;
};

[[nodiscard]] caors::error
make_external_memory_source(caors::assets::asset_manager::source_factory_context& context,
                            const caors::assets::project_source_config& config, void* user_data) {
  if (user_data == nullptr) {
    return {caors::error_domain::resources, caors::errc::invalid_argument};
  }

  auto& state = *static_cast<external_source_factory_state*>(user_data);
  ++state.calls;
  state.resource = context.resource;
  if (const caors::variant* value =
          config.options.find_member_variant(caors::string_name{"mount"})) {
    if (const caors::string* text = value->try_as_string()) {
      state.mount.assign(text->begin(), text->end());
    }
  }
  if (const caors::variant* value =
          config.options.find_member_variant(caors::string_name{"read_only"})) {
    if (const bool* enabled = value->try_as_bool()) {
      state.read_only = *enabled;
    }
  }

  state.file_system = context.manager.create_owned<caors::io::memory_file_system>(context.resource);
  if (state.file_system == nullptr) {
    return {caors::error_domain::resources, caors::errc::out_of_memory};
  }

  state.source = context.manager.create_owned<caors::resources::file_source>(*state.file_system);
  if (state.source == nullptr) {
    return {caors::error_domain::resources, caors::errc::out_of_memory};
  }

  return context.manager.bind_source(config.name, *state.source);
}

[[nodiscard]] caors::error
make_external_serialized_type(caors::assets::asset_manager::type_factory_context& context,
                              const caors::assets::project_resource_type_config& config,
                              void* user_data) {
  if (user_data == nullptr) {
    return {caors::error_domain::resources, caors::errc::invalid_argument};
  }

  if (!config.type || config.cache != caors::string_name{"external_memory"} ||
      config.loader != caors::string_name{"external_json"}) {
    return {caors::error_domain::resources, caors::errc::unsupported};
  }

  auto& state = *static_cast<external_type_factory_state*>(user_data);
  ++state.calls;
  if (const caors::variant* value =
          config.options.find_member_variant(caors::string_name{"format"})) {
    if (const caors::string* text = value->try_as_string()) {
      state.format.assign(text->begin(), text->end());
    }
  }
  if (const caors::variant* priority =
          config.options.find_member_variant(caors::string_name{"priority"})) {
    if (const std::uint64_t* value = priority->try_as_u64()) {
      state.priority = *value;
    } else if (const std::int64_t* signed_value = priority->try_as_i64();
               signed_value != nullptr && *signed_value >= 0) {
      state.priority = static_cast<std::uint64_t>(*signed_value);
    }
  }

  state.cache =
      context.manager.create_owned<caors::resources::cache<serialized_resource>>(context.resource);
  if (state.cache == nullptr) {
    return {caors::error_domain::resources, caors::errc::out_of_memory};
  }

  state.loader = context.manager.create_owned<caors::assets::json::loader<serialized_resource>>(
      context.provider);
  if (state.loader == nullptr) {
    return {caors::error_domain::resources, caors::errc::out_of_memory};
  }

  return context.database.register_type(*state.cache, *state.loader, config.type);
}

[[nodiscard]] caors::error
make_counted_source(caors::assets::asset_manager::source_factory_context& context,
                    const caors::assets::project_source_config& config, void* user_data) {
  if (user_data == nullptr) {
    return {caors::error_domain::resources, caors::errc::invalid_argument};
  }

  auto& state = *static_cast<counted_source_factory_state*>(user_data);
  ++state.calls;

  auto* source = context.manager.create_owned<counted_source>(&state.destroyed);
  if (source == nullptr) {
    return {caors::error_domain::resources, caors::errc::out_of_memory};
  }

  return context.manager.bind_source(config.name, *source);
}

} // namespace

TEST_CASE("resources - asset_manager builds provider manifest and dependency graph from config",
          "[resources]") {
  require_serialized_resource_metadata();

  caors::assets::project_config config;
  REQUIRE(caors::assets::json::load_project_config(config, make_project_config_json()).succeeded());
  REQUIRE(config.sources.size() == 1);
  REQUIRE(config.resource_types.size() == 1);
  REQUIRE(config.resources.size() == 2);
  REQUIRE(config.dependencies.size() == 1);
  REQUIRE(config.preload_roots.size() == 2);
  CHECK(config.schema_version == 1);
  CHECK(config.sources[0].name == std::string_view{"assets"});
  CHECK(config.sources[0].type == std::string_view{"memory_file_system"});
  REQUIRE(config.sources[0].options.is_object());
  REQUIRE(config.sources[0].options.find_member_variant(caors::string_name{"mount"}) != nullptr);
  CHECK(config.sources[0].options.find_member_variant(caors::string_name{"mount"})->as_string() ==
        std::string_view{"debug-assets"});
  CHECK(config.resource_types[0].type == serialized_manifest_type());
  CHECK(config.resource_types[0].loader == std::string_view{"json"});
  CHECK(config.resource_types[0].cache == std::string_view{"memory"});
  REQUIRE(config.resource_types[0].options.is_object());
  REQUIRE(config.resource_types[0].options.find_member_variant(caors::string_name{"format"}) !=
          nullptr);
  CHECK(config.resource_types[0]
            .options.find_member_variant(caors::string_name{"format"})
            ->as_string() == std::string_view{"resource-json"});
  CHECK(config.preload_roots[0] == test_guid_b);
  CHECK(config.preload_roots[1] == test_guid_c);

  caors::io::memory_file_system files;
  REQUIRE(files.write_all("config/project.json", make_project_config_json()).succeeded());
  REQUIRE(
      files.write_all("assets/resource-a.json", std::string_view{R"({"width":512})"}).succeeded());
  REQUIRE(
      files.write_all("assets/resource-b.json", std::string_view{R"({"width":1024})"}).succeeded());

  caors::resources::file_source source{files};
  caors::assets::project_config loaded_from_source;
  REQUIRE(
      caors::assets::json::load_project_config(loaded_from_source, source, "config/project.json")
          .succeeded());
  CHECK(loaded_from_source.resources.size() == 2);

  caors::assets::asset_manager assets;
  REQUIRE(assets.bind_source(caors::string_name{"assets"}, source).succeeded());
  REQUIRE(assets.build(config).succeeded());

  const caors::resources::manifest_entry* entry_a = assets.manifest().find(test_guid_a);
  REQUIRE(entry_a != nullptr);
  CHECK(entry_a->location.string_view() == std::string_view{"assets/resource-a.json"});
  CHECK(entry_a->type == serialized_manifest_type());
  CHECK(std::string_view{entry_a->hash.data(), entry_a->hash.size()} == "hash-a");
  CHECK(entry_a->version == 1);
  CHECK(assets.provider().contains(test_guid_a));
  CHECK(assets.provider().find(test_guid_b).location.string_view() ==
        std::string_view{"assets/resource-b.json"});
  CHECK(assets.dependency_graph().contains(test_guid_b, test_guid_a));

  caors::assets::json::loader<serialized_resource> loader{assets.provider()};
  caors::resource_cache<serialized_resource> cache;
  REQUIRE(assets.register_type(cache, loader, serialized_manifest_type()).succeeded());

  caors::resources::preload_result loaded = assets.preload(test_guid_b);
  REQUIRE(loaded.succeeded());
  REQUIRE(loaded.loaded.size() == 2);
  CHECK(loaded.loaded[0] ==
        caors::resources::preload_item{test_guid_a, serialized_manifest_type()});
  CHECK(loaded.loaded[1] ==
        caors::resources::preload_item{test_guid_b, serialized_manifest_type()});

  const serialized_resource* resource_a = cache.find(test_guid_a);
  const serialized_resource* resource_b = cache.find(test_guid_b);
  REQUIRE(resource_a != nullptr);
  REQUIRE(resource_b != nullptr);
  CHECK(resource_a->width == 512);
  CHECK(resource_b->width == 1024);
}

TEST_CASE("resources - asset_manager creates configured source cache and loader from factories",
          "[resources]") {
  require_serialized_resource_metadata();

  caors::assets::project_config config;
  REQUIRE(caors::assets::json::load_project_config(config, make_project_config_json()).succeeded());

  caors::assets::asset_manager assets;
  REQUIRE(caors::assets::json::register_type_factory<serialized_resource>(
              assets, serialized_manifest_type())
              .succeeded());
  REQUIRE(assets.build(config).succeeded());

  auto* source = dynamic_cast<caors::resources::file_source*>(
      assets.find_source(caors::string_name{"assets"}));
  REQUIRE(source != nullptr);

  auto* files = dynamic_cast<caors::io::memory_file_system*>(&source->file_system());
  REQUIRE(files != nullptr);
  REQUIRE(
      files->write_all("assets/resource-a.json", std::string_view{R"({"width":512})"}).succeeded());
  REQUIRE(files->write_all("assets/resource-b.json", std::string_view{R"({"width":1024})"})
              .succeeded());

  caors::resources::preload_result loaded = assets.preload(test_guid_b);
  REQUIRE(loaded.succeeded());
  CHECK(loaded.loaded.size() == 2);

  const serialized_resource* resource_a = assets.database().find<serialized_resource>(test_guid_a);
  const serialized_resource* resource_b = assets.database().find<serialized_resource>(test_guid_b);
  REQUIRE(resource_a != nullptr);
  REQUIRE(resource_b != nullptr);
  CHECK(resource_a->width == 512);
  CHECK(resource_b->width == 1024);
}

TEST_CASE("resources - asset_manager exposes high level resource operations", "[resources]") {
  require_serialized_resource_metadata();

  caors::assets::project_config config;
  REQUIRE(caors::assets::json::load_project_config(config, make_project_config_json()).succeeded());

  caors::assets::asset_manager assets;
  REQUIRE((
      caors::assets::register_cached_type_factory<serialized_resource,
                                                  caors::assets::json::loader<serialized_resource>>(
          assets, serialized_manifest_type(), caors::string_name{"memory"},
          caors::string_name{"json"})
          .succeeded()));
  REQUIRE(assets.build(config).succeeded());

  auto* source = dynamic_cast<caors::resources::file_source*>(
      assets.find_source(caors::string_name{"assets"}));
  REQUIRE(source != nullptr);

  auto* files = dynamic_cast<caors::io::memory_file_system*>(&source->file_system());
  REQUIRE(files != nullptr);
  REQUIRE(
      files->write_all("assets/resource-a.json", std::string_view{R"({"width":512})"}).succeeded());
  REQUIRE(files->write_all("assets/resource-b.json", std::string_view{R"({"width":1024})"})
              .succeeded());

  caors::result<serialized_resource*> loaded = assets.load<serialized_resource>(test_guid_a);
  REQUIRE(loaded.succeeded());
  REQUIRE(loaded.value != nullptr);
  CHECK(loaded.value->width == 512);
  CHECK(assets.find<serialized_resource>(test_guid_a) == loaded.value);
  CHECK(assets.find(test_guid_a) == loaded.value);

  caors::resource_ref<serialized_resource> reference{test_guid_a};
  REQUIRE(assets.resolve(reference));
  CHECK(reference.get() == loaded.value);
  CHECK(assets.is_current(reference));

  REQUIRE(files->write_all("assets/resource-a.json", std::string_view{R"({"width":2048})"})
              .succeeded());
  caors::result<serialized_resource*> reloaded = assets.reload<serialized_resource>(test_guid_a);
  REQUIRE(reloaded.succeeded());
  REQUIRE(reloaded.value != nullptr);
  CHECK(reloaded.value->width == 2048);
  CHECK_FALSE(assets.is_current(reference));
  REQUIRE(assets.resolve(reference));
  CHECK(reference.get() == reloaded.value);

  reloaded.value->width = 4096;
  caors::assets::json::saver<serialized_resource> saver{assets.provider()};
  REQUIRE(saver.save(*reloaded.value).succeeded());
  caors::io::byte_buffer_result saved = files->read_all("assets/resource-a.json");
  REQUIRE(saved.succeeded());
  const std::string_view saved_text{reinterpret_cast<const char*>(saved.value.data()),
                                    saved.value.size()};
  CHECK(saved_text == std::string_view{R"({"width":4096})"});

  caors::resources::resource_dependency_list dependencies;
  dependencies.collect(test_guid_b, caors::meta::id_of<serialized_resource>());
  CHECK(assets.replace_dependencies(test_guid_a, dependencies) == 1);
  caors::resource_dependency_graph::guid_list affected = assets.affected_by(test_guid_b);
  REQUIRE(affected.size() == 1);
  CHECK(affected[0] == test_guid_a);
}

TEST_CASE("resources - asset_manager forwards async requests", "[resources]") {
  require_serialized_resource_metadata();

  caors::assets::project_config config;
  REQUIRE(caors::assets::json::load_project_config(config, make_project_config_json()).succeeded());

  caors::assets::asset_manager assets;
  REQUIRE(caors::assets::json::register_type_factory<serialized_resource>(
              assets, serialized_manifest_type())
              .succeeded());
  REQUIRE(assets.build(config).succeeded());

  auto* source = dynamic_cast<caors::resources::file_source*>(
      assets.find_source(caors::string_name{"assets"}));
  REQUIRE(source != nullptr);

  auto* files = dynamic_cast<caors::io::memory_file_system*>(&source->file_system());
  REQUIRE(files != nullptr);
  REQUIRE(
      files->write_all("assets/resource-a.json", std::string_view{R"({"width":512})"}).succeeded());
  REQUIRE(files->write_all("assets/resource-b.json", std::string_view{R"({"width":1024})"})
              .succeeded());

  caors::tasks::inline_executor executor;
  caors::tasks::completion_queue completions;

  caors::resources::load_request<serialized_resource> loaded =
      assets.request_load<serialized_resource>(test_guid_a, executor, completions);
  REQUIRE(loaded);
  CHECK(loaded.status() == caors::resources::request_status::loading);
  CHECK(completions.drain() == 1);
  REQUIRE(loaded.loaded());
  REQUIRE(loaded.try_get());
  CHECK(loaded.try_get()->width == 512);

  REQUIRE(
      files->write_all("assets/resource-a.json", std::string_view{R"({"width":256})"}).succeeded());
  caors::resources::load_request<serialized_resource> reloaded =
      assets.request_reload<serialized_resource>(test_guid_a, executor, completions);
  REQUIRE(reloaded);
  CHECK(reloaded.status() == caors::resources::request_status::loading);
  CHECK(completions.drain() == 1);
  REQUIRE(reloaded.loaded());
  CHECK(reloaded.try_get()->width == 256);

  caors::resources::preload_request preloaded =
      assets.request_preload(test_guid_b, executor, completions);
  REQUIRE(preloaded);
  CHECK(completions.drain() == 1);
  REQUIRE(preloaded.loaded());
  caors::resources::preload_result result = preloaded.result();
  REQUIRE(result.succeeded());
  REQUIRE(result.loaded.size() == 1);
  CHECK(result.loaded[0] ==
        caors::resources::preload_item{test_guid_b, serialized_manifest_type()});
  CHECK(assets.find<serialized_resource>(test_guid_b)->width == 1024);
}

TEST_CASE("resources - asset_manager accepts external source factory state", "[resources]") {
  caors::assets::project_config config;
  REQUIRE(caors::assets::json::load_project_config(config, make_project_config_json()).succeeded());
  config.sources[0].type = caors::string_name{"external_memory"};

  external_source_factory_state state;
  external_source_factory_state other_state;
  caors::assets::factory_option_schema schema;
  schema.require(caors::string_name{"mount"}, caors::assets::factory_option_kind::string)
      .optional(caors::string_name{"read_only"}, caors::assets::factory_option_kind::boolean);
  schema.allow_unknown = false;

  caors::assets::asset_manager assets;
  REQUIRE(assets
              .register_source_factory(caors::string_name{"external_memory"},
                                       &make_external_memory_source, &state, schema)
              .succeeded());
  REQUIRE(assets
              .register_source_factory(caors::string_name{"external_memory"},
                                       &make_external_memory_source, &state, schema)
              .succeeded());
  CHECK(assets.register_source_factory(caors::string_name{"external_memory"},
                                       &make_external_memory_source, &other_state) ==
        caors::error{caors::error_domain::resources, caors::errc::already_exists});

  REQUIRE(assets.build(config).succeeded());
  CHECK(state.calls == 1);
  CHECK(state.resource == assets.resource());
  CHECK(state.mount == "debug-assets");
  CHECK(state.read_only);
  REQUIRE(state.file_system != nullptr);
  REQUIRE(state.source != nullptr);
  CHECK(assets.find_source(caors::string_name{"assets"}) == state.source);
}

TEST_CASE("resources - asset_manager validates source factory option schema", "[resources]") {
  caors::assets::project_config config;
  REQUIRE(
      caors::assets::json::load_project_config(
          config,
          R"({"sources":[{"name":"assets","type":"external_memory","options":{"read_only":true}}]})")
          .succeeded());

  caors::assets::factory_option_schema schema;
  schema.require(caors::string_name{"mount"}, caors::assets::factory_option_kind::string)
      .optional(caors::string_name{"read_only"}, caors::assets::factory_option_kind::boolean);
  schema.allow_unknown = false;

  external_source_factory_state state;
  caors::assets::asset_manager assets;
  REQUIRE(assets
              .register_source_factory(caors::string_name{"external_memory"},
                                       &make_external_memory_source, &state, schema)
              .succeeded());

  caors::assets::asset_build_result result = assets.build_detailed(config);
  CHECK(result.status ==
        caors::error{caors::error_domain::resources, caors::errc::invalid_argument});
  CHECK(result.stage == caors::assets::asset_build_stage::source);
  CHECK(result.index == 0);
  CHECK(result.name == caors::string_name{"assets"});
  CHECK(result.type == caors::string_name{"external_memory"});
  REQUIRE_FALSE(result.reasons.empty());
  CHECK(result.reasons.front().find("mount") != caors::string::npos);
  CHECK(state.calls == 0);
}

TEST_CASE("resources - asset_manager accepts external type factory state", "[resources]") {
  require_serialized_resource_metadata();

  caors::assets::project_config config;
  REQUIRE(caors::assets::json::load_project_config(config, make_project_config_json()).succeeded());
  config.resource_types[0].cache = caors::string_name{"external_memory"};
  config.resource_types[0].loader = caors::string_name{"external_json"};

  external_type_factory_state state;
  caors::assets::factory_option_schema schema;
  schema.require(caors::string_name{"format"}, caors::assets::factory_option_kind::string)
      .optional(caors::string_name{"priority"});

  caors::assets::asset_manager assets;
  REQUIRE(assets
              .register_type_factory(serialized_manifest_type(), &make_external_serialized_type,
                                     &state, schema)
              .succeeded());
  REQUIRE(assets.build(config).succeeded());

  CHECK(state.calls == 1);
  CHECK(state.format == "resource-json");
  CHECK(state.priority == 2);
  REQUIRE(state.cache != nullptr);
  REQUIRE(state.loader != nullptr);

  auto* source = dynamic_cast<caors::resources::file_source*>(
      assets.find_source(caors::string_name{"assets"}));
  REQUIRE(source != nullptr);

  auto* files = dynamic_cast<caors::io::memory_file_system*>(&source->file_system());
  REQUIRE(files != nullptr);
  REQUIRE(
      files->write_all("assets/resource-a.json", std::string_view{R"({"width":128})"}).succeeded());
  REQUIRE(
      files->write_all("assets/resource-b.json", std::string_view{R"({"width":256})"}).succeeded());

  caors::resources::preload_result loaded = assets.preload(test_guid_b);
  REQUIRE(loaded.succeeded());
  CHECK(loaded.loaded.size() == 2);
  REQUIRE(state.cache->find(test_guid_a) != nullptr);
  REQUIRE(state.cache->find(test_guid_b) != nullptr);
  CHECK(state.cache->find(test_guid_a)->width == 128);
  CHECK(state.cache->find(test_guid_b)->width == 256);
}

TEST_CASE("resources - asset_manager validates type factory option schema", "[resources]") {
  require_serialized_resource_metadata();

  caors::assets::project_config config;
  REQUIRE(
      caors::assets::json::load_project_config(
          config,
          R"({"resource_types":[{"type":"game.serialized","loader":"external_json","cache":"external_memory","options":{"priority":2}}]})")
          .succeeded());

  external_type_factory_state state;
  caors::assets::factory_option_schema schema;
  schema.require(caors::string_name{"format"}, caors::assets::factory_option_kind::string)
      .optional(caors::string_name{"priority"});

  caors::assets::asset_manager assets;
  REQUIRE(assets
              .register_type_factory(serialized_manifest_type(), &make_external_serialized_type,
                                     &state, schema)
              .succeeded());

  caors::assets::asset_build_result result = assets.build_detailed(config);
  CHECK(result.status ==
        caors::error{caors::error_domain::resources, caors::errc::invalid_argument});
  CHECK(result.stage == caors::assets::asset_build_stage::resource_type);
  CHECK(result.index == 0);
  CHECK(result.name == serialized_manifest_type());
  CHECK(result.type == serialized_manifest_type());
  REQUIRE_FALSE(result.reasons.empty());
  CHECK(result.reasons.front().find("format") != caors::string::npos);
  CHECK(state.calls == 0);
}

TEST_CASE("resources - asset_manager rebuild clears config created runtime objects",
          "[resources]") {
  caors::assets::project_config config;
  caors::assets::project_source_config source{config.resource};
  source.name = caors::string_name{"assets"};
  source.type = caors::string_name{"counted"};
  config.sources.push_back(std::move(source));

  counted_source_factory_state state;

  {
    caors::assets::asset_manager assets;
    REQUIRE(
        assets.register_source_factory(caors::string_name{"counted"}, &make_counted_source, &state)
            .succeeded());

    REQUIRE(assets.build(config).succeeded());
    CHECK(state.calls == 1);
    CHECK(state.destroyed == 0);
    REQUIRE(assets.find_source(caors::string_name{"assets"}) != nullptr);

    REQUIRE(assets.build(config).succeeded());
    CHECK(state.calls == 2);
    CHECK(state.destroyed == 1);
    REQUIRE(assets.find_source(caors::string_name{"assets"}) != nullptr);
  }

  CHECK(state.destroyed == 2);
}

TEST_CASE("resources - asset_manager rebuild keeps manual type bindings", "[resources]") {
  require_serialized_resource_metadata();

  caors::assets::project_config config;
  REQUIRE(caors::assets::json::load_project_config(config, make_project_config_json()).succeeded());

  caors::io::memory_file_system files;
  REQUIRE(
      files.write_all("assets/resource-a.json", std::string_view{R"({"width":512})"}).succeeded());
  REQUIRE(
      files.write_all("assets/resource-b.json", std::string_view{R"({"width":1024})"}).succeeded());

  caors::resources::file_source source{files};
  caors::resource_cache<serialized_resource> cache;

  caors::assets::asset_manager assets;
  caors::assets::json::loader<serialized_resource> loader{assets.provider()};
  REQUIRE(assets.bind_source(caors::string_name{"assets"}, source).succeeded());
  REQUIRE(assets.register_type(cache, loader, serialized_manifest_type()).succeeded());

  REQUIRE(assets.build(config).succeeded());
  caors::resources::preload_result loaded = assets.preload(test_guid_b);
  REQUIRE(loaded.succeeded());
  REQUIRE(cache.find(test_guid_a) != nullptr);

  REQUIRE(assets.build(config).succeeded());
  CHECK(assets.find<serialized_resource>(test_guid_a) == cache.find(test_guid_a));
  CHECK(assets.find<serialized_resource>(test_guid_b) == cache.find(test_guid_b));
}

TEST_CASE("resources - asset_manager reports unsupported configured runtime strategy",
          "[resources]") {
  caors::assets::project_config config;
  REQUIRE(caors::assets::json::load_project_config(config, make_project_config_json()).succeeded());
  config.resource_types[0].loader = caors::string_name{"binary"};

  caors::assets::asset_manager assets;
  REQUIRE(caors::assets::json::register_type_factory<serialized_resource>(
              assets, serialized_manifest_type())
              .succeeded());

  const caors::assets::asset_build_result result = assets.build_detailed(config);
  CHECK(result.status == caors::error{caors::error_domain::resources, caors::errc::unsupported});
  CHECK(result.stage == caors::assets::asset_build_stage::resource_type);
  CHECK(result.index == 0);
  CHECK(result.name == serialized_manifest_type());
  REQUIRE_FALSE(result.reasons.empty());
}

TEST_CASE("resources - asset_manager reports missing configured sources", "[resources]") {
  caors::assets::project_config config;
  REQUIRE(caors::assets::json::load_project_config(config, make_project_config_json()).succeeded());
  config.sources[0].type = caors::string_name{"unknown_source"};

  caors::assets::asset_manager assets;
  const caors::assets::asset_build_result result = assets.build_detailed(config);
  CHECK(result.status == caors::error{caors::error_domain::resources, caors::errc::not_found});
  CHECK(result.stage == caors::assets::asset_build_stage::source);
  CHECK(result.index == 0);
  CHECK(result.name == caors::string_name{"assets"});
  CHECK(result.type == caors::string_name{"unknown_source"});
  REQUIRE_FALSE(result.reasons.empty());
}

TEST_CASE("resources - project config validates schema and options", "[resources]") {
  caors::assets::project_config config;

  CHECK(caors::assets::json::load_project_config(config, R"({"schema_version":2})") ==
        caors::error{caors::error_domain::resources, caors::errc::unsupported});

  CHECK(caors::assets::json::load_project_config(
            config, R"({"sources":[{"name":"assets","type":"memory","options":"bad"}]})") ==
        caors::error{caors::error_domain::resources, caors::errc::invalid_argument});

  CHECK(
      caors::assets::json::load_project_config(
          config,
          R"({"resource_types":[{"type":"game.serialized","loader":"json","cache":"memory","options":["bad"]}]})") ==
      caors::error{caors::error_domain::resources, caors::errc::invalid_argument});
}
