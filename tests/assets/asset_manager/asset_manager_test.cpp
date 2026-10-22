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

[[nodiscard]] caors::string_name secondary_manifest_type() {
  return caors::string_name{"game.secondary"};
}

class secondary_serialized_resource : public caors::resource {
public:
  using caors::resource::resource;

  int width = 0;
};

void require_secondary_serialized_resource_metadata() {
  REQUIRE(serialization_result_ok(caors::registration::type_<secondary_serialized_resource>(
                                      "caors.test.resources.secondary_serialized_resource")
                                      .meta()
                                      .property("width", &secondary_serialized_resource::width)
                                      .serialization()
                                      .commit()));
}

[[nodiscard]] bool contains_reason(const caors::assets::asset_build_result::reason_list& reasons,
                                   std::string_view text) {
  for (const caors::string& reason : reasons) {
    if (std::string_view{reason.data(), reason.size()}.find(text) != std::string_view::npos) {
      return true;
    }
  }

  return false;
}

[[nodiscard]] bool
contains_reason(const caors::serialization::config_validation_result::diagnostic_list& diagnostics,
                std::string_view text) {
  for (const caors::serialization::config_validation_diagnostic& diagnostic : diagnostics) {
    const caors::string& reason = diagnostic.reason;
    if (std::string_view{reason.data(), reason.size()}.find(text) != std::string_view::npos) {
      return true;
    }
  }

  return false;
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

[[nodiscard]] std::string make_multi_source_type_config_json() {
  std::string json{R"({
    "sources": [
      {
        "name": "assets",
        "type": "memory_file_system"
      },
      {
        "name": "dlc",
        "type": "memory_file_system"
      }
    ],
    "resource_types": [
      {
        "type": "game.serialized",
        "loader": "json",
        "cache": "memory"
      },
      {
        "type": "game.secondary",
        "loader": "json",
        "cache": "memory"
      }
    ],
    "resources": [
      {
        "guid": ")"};
  json += test_guid_a.string();
  json += R"(",
        "source": "assets",
        "location": "base/resource-a.json",
        "type": "game.serialized"
      },
      {
        "guid": ")";
  json += test_guid_c.string();
  json += R"(",
        "source": "dlc",
        "location": "packs/resource-c.json",
        "type": "game.secondary"
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
  int priority = 0;
  std::string filter;
  std::string mount;
};

struct external_file_system_factory_state {
  int calls = 0;
  caors::memory_resource* resource = nullptr;
  caors::io::memory_file_system* file_system = nullptr;
  std::string label;
};

struct typed_external_file_system_config {
  std::string label;
};

void require_typed_external_file_system_config_schema() {
  REQUIRE(serialization_result_ok(caors::registration::type_<typed_external_file_system_config>(
                                      "caors.test.assets.typed_external_file_system_config")
                                      .meta()
                                      .property("label", &typed_external_file_system_config::label,
                                                caors::meta::registration::required(),
                                                caors::meta::registration::non_empty())
                                      .serialization()
                                      .commit()));
}

struct typed_external_source_config {
  std::string mount;
  bool read_only = false;
  int priority = 0;
  std::string filter;
};

void require_typed_external_source_config_schema() {
  REQUIRE(serialization_result_ok(
      caors::registration::type_<typed_external_source_config>(
          "caors.test.assets.typed_external_source_config")
          .meta()
          .property("mount", &typed_external_source_config::mount,
                    caors::meta::registration::required(), caors::meta::registration::non_empty())
          .property("read_only", &typed_external_source_config::read_only)
          .property("priority", &typed_external_source_config::priority,
                    caors::meta::registration::range(1, 10),
                    caors::meta::registration::default_value(3))
          .property("filter", &typed_external_source_config::filter,
                    caors::meta::registration::one_of(
                        {std::string_view{"nearest"}, std::string_view{"linear"}}),
                    caors::meta::registration::default_value("nearest"))
          .serialization()
          .commit()));
  const caors::serialization::schema_view schema =
      caors::serialization::find_schema(caors::meta::resolve<typed_external_source_config>());
  REQUIRE(schema.field_count() == 4);
}

struct external_type_factory_state {
  int calls = 0;
  caors::resources::cache<serialized_resource>* cache = nullptr;
  caors::assets::json::sync_loader<serialized_resource>* loader = nullptr;
  std::string format;
  std::uint64_t priority = 0;
};

struct typed_external_type_config {
  std::string format;
  int priority = 0;
};

void require_typed_external_type_config_schema() {
  REQUIRE(serialization_result_ok(
      caors::registration::type_<typed_external_type_config>(
          "caors.test.assets.typed_external_type_config")
          .meta()
          .property("format", &typed_external_type_config::format,
                    caors::meta::registration::required(), caors::meta::registration::non_empty(),
                    caors::meta::registration::one_of(
                        {std::string_view{"resource-json"}, std::string_view{"resource-binary"}}))
          .property("priority", &typed_external_type_config::priority,
                    caors::meta::registration::range(1, 100),
                    caors::meta::registration::default_value(1))
          .serialization()
          .commit()));
}

class counted_source final : public caors::resources::source {
public:
  explicit counted_source(int* destroyed) noexcept : destroyed_(destroyed) {}
  ~counted_source() override {
    if (destroyed_ != nullptr) {
      ++*destroyed_;
    }
  }

  [[nodiscard]] bool exists(caors::io::location_view) const override { return false; }

  [[nodiscard]] caors::io::byte_buffer_result read(caors::io::location_view,
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
make_external_memory_source(caors::assets::project_builder::source_factory_context& context,
                            const caors::assets::project_source_config& config, void* user_data) {
  if (user_data == nullptr) {
    return {caors::error_domain::resources, caors::errc::invalid_argument};
  }

  auto& state = *static_cast<external_source_factory_state*>(user_data);
  ++state.calls;
  state.resource = context.resource;
  const caors::assets::factory_options_view options{config.options};
  (void)options.read(caors::string_name{"mount"}, state.mount);
  (void)options.read(caors::string_name{"read_only"}, state.read_only);

  state.file_system =
      context.project().create_owned<caors::io::memory_file_system>(context.resource);
  if (state.file_system == nullptr) {
    return {caors::error_domain::resources, caors::errc::out_of_memory};
  }

  state.source = context.project().create_owned<caors::resources::file_source>(*state.file_system);
  if (state.source == nullptr) {
    return {caors::error_domain::resources, caors::errc::out_of_memory};
  }

  return context.project().bind_source(config.name, *state.source);
}

[[nodiscard]] caors::error make_typed_external_memory_file_system(
    caors::assets::project_builder::file_system_factory_context& context,
    const caors::assets::project_file_system_config& config,
    const typed_external_file_system_config& options, void* user_data) {
  if (user_data == nullptr) {
    return {caors::error_domain::resources, caors::errc::invalid_argument};
  }

  auto& state = *static_cast<external_file_system_factory_state*>(user_data);
  ++state.calls;
  state.resource = context.resource;
  state.label = options.label;

  auto files = std::allocate_shared<caors::io::memory_file_system>(
      caors::allocator<caors::io::memory_file_system>{context.resource}, context.resource);
  state.file_system = files.get();
  return context.project().bind_file_system(config.name, std::move(files));
}

[[nodiscard]] caors::error
make_typed_external_memory_source(caors::assets::project_builder::source_factory_context& context,
                                  const caors::assets::project_source_config& config,
                                  const typed_external_source_config& options, void* user_data) {
  if (user_data == nullptr) {
    return {caors::error_domain::resources, caors::errc::invalid_argument};
  }

  auto& state = *static_cast<external_source_factory_state*>(user_data);
  ++state.calls;
  state.resource = context.resource;

  state.mount = options.mount;
  state.read_only = options.read_only;
  state.priority = options.priority;
  state.filter = options.filter;
  state.file_system =
      context.project().create_owned<caors::io::memory_file_system>(context.resource);
  if (state.file_system == nullptr) {
    return {caors::error_domain::resources, caors::errc::out_of_memory};
  }

  state.source = context.project().create_owned<caors::resources::file_source>(*state.file_system);
  if (state.source == nullptr) {
    return {caors::error_domain::resources, caors::errc::out_of_memory};
  }

  return context.project().bind_source(config.name, *state.source);
}

[[nodiscard]] caors::error
make_external_serialized_type(caors::assets::project_builder::type_factory_context& context,
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
  const caors::assets::factory_options_view options{config.options};
  (void)options.read(caors::string_name{"format"}, state.format);
  (void)options.read(caors::string_name{"priority"}, state.priority);

  state.cache = context.project().create_owned<caors::resources::cache<serialized_resource>>(
      context.resource);
  if (state.cache == nullptr) {
    return {caors::error_domain::resources, caors::errc::out_of_memory};
  }

  state.loader =
      context.project().create_owned<caors::assets::json::sync_loader<serialized_resource>>(
          context.provider);
  if (state.loader == nullptr) {
    return {caors::error_domain::resources, caors::errc::out_of_memory};
  }

  return context.database.register_type(*state.cache, *state.loader, config.type);
}

[[nodiscard]] caors::error
make_typed_external_serialized_type(caors::assets::project_builder::type_factory_context& context,
                                    const caors::assets::project_resource_type_config& config,
                                    const typed_external_type_config& options, void* user_data) {
  if (user_data == nullptr) {
    return {caors::error_domain::resources, caors::errc::invalid_argument};
  }

  auto& state = *static_cast<external_type_factory_state*>(user_data);
  ++state.calls;
  state.format = options.format;
  state.priority = static_cast<std::uint64_t>(options.priority);

  state.cache = context.project().create_owned<caors::resources::cache<serialized_resource>>(
      context.resource);
  if (state.cache == nullptr) {
    return {caors::error_domain::resources, caors::errc::out_of_memory};
  }

  state.loader =
      context.project().create_owned<caors::assets::json::sync_loader<serialized_resource>>(
          context.provider);
  if (state.loader == nullptr) {
    return {caors::error_domain::resources, caors::errc::out_of_memory};
  }

  return context.database.register_type(*state.cache, *state.loader, config.type);
}

[[nodiscard]] caors::error
make_counted_source(caors::assets::project_builder::source_factory_context& context,
                    const caors::assets::project_source_config& config, void* user_data) {
  if (user_data == nullptr) {
    return {caors::error_domain::resources, caors::errc::invalid_argument};
  }

  auto& state = *static_cast<counted_source_factory_state*>(user_data);
  ++state.calls;

  auto* source = context.project().create_owned<counted_source>(&state.destroyed);
  if (source == nullptr) {
    return {caors::error_domain::resources, caors::errc::out_of_memory};
  }

  return context.project().bind_source(config.name, *source);
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
  REQUIRE(caors::assets::project_builder{assets}
              .bind_source(caors::string_name{"assets"}, source)
              .succeeded());
  REQUIRE(caors::assets::project_builder{assets}.build(config).succeeded());

  const caors::resources::manifest_entry* entry_a = assets.manifest().find(test_guid_a);
  REQUIRE(entry_a != nullptr);
  CHECK(entry_a->location.string_view() == std::string_view{"assets/resource-a.json"});
  CHECK(entry_a->type == serialized_manifest_type());
  CHECK(std::string_view{entry_a->hash.data(), entry_a->hash.size()} == "hash-a");
  CHECK(entry_a->version == 1);
  CHECK(caors::assets::project_builder{assets}.provider().contains(test_guid_a));
  CHECK(
      caors::assets::project_builder{assets}.provider().find(test_guid_b).location.string_view() ==
      std::string_view{"assets/resource-b.json"});
  CHECK(assets.dependency_graph().contains(test_guid_b, test_guid_a));

  caors::assets::json::sync_loader<serialized_resource> loader{
      caors::assets::project_builder{assets}.provider()};
  caors::resource_cache<serialized_resource> cache;
  REQUIRE(assets.register_type(cache, loader, serialized_manifest_type()).succeeded());

  caors::resources::preload_result loaded =
      assets.create_group(caors::tasks::inline_executor(), caors::tasks::inline_executor())
          .request_preload(test_guid_b)
          .result();
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

TEST_CASE("resources - project config preserves uri resource locations", "[resources]") {
  std::string json{R"({
    "resources": [
      {
        "guid": ")"};
  json += test_guid_a.string();
  json += R"(",
        "source": "assets",
        "location": "caors://assets//textures/albedo.json",
        "type": "game.serialized"
      }
    ]
  })";

  caors::assets::project_config config;
  REQUIRE(caors::assets::json::load_project_config(config, json).succeeded());
  REQUIRE(config.resources.size() == 1);
  CHECK(config.resources[0].location.string_view() ==
        std::string_view{"caors://assets/textures/albedo.json"});
}

TEST_CASE("resources - asset_manager builds configured file systems and file source",
          "[resources]") {
  require_serialized_resource_metadata();

  std::string json{R"({
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
        "file_system": "vfs"
      }
    ],
    "resource_types": [
      {
        "type": "game.serialized",
        "loader": "json",
        "cache": "memory"
      }
    ],
    "resources": [
      {
        "guid": ")"};
  json += test_guid_a.string();
  json += R"(",
        "source": "assets",
        "location": "caors://assets//resource-a.json",
        "type": "game.serialized"
      }
    ]
  })";

  caors::assets::project_config config;
  REQUIRE(caors::assets::json::load_project_config(config, json).succeeded());
  REQUIRE(config.file_systems.size() == 2);
  CHECK(config.sources.size() == 1);
  CHECK(config.sources[0].type == std::string_view{"file_system"});
  CHECK(config.sources[0].file_system == std::string_view{"vfs"});
  CHECK(config.file_systems[1].routes.size() == 1);
  CHECK(config.file_systems[1].routes[0].prefix.string_view() ==
        std::string_view{"caors://assets"});

  caors::assets::asset_manager assets;
  REQUIRE(caors::assets::json::register_type_factory<serialized_resource>(
              caors::assets::project_builder{assets}, serialized_manifest_type())
              .succeeded());
  REQUIRE(caors::assets::project_builder{assets}.build(config).succeeded());
  CHECK(caors::assets::project_builder{assets}.file_system_count() == 2);
  CHECK(caors::assets::project_builder{assets}.source_count() == 1);

  auto* files = dynamic_cast<caors::io::memory_file_system*>(
      caors::assets::project_builder{assets}.find_file_system(caors::string_name{"memory"}));
  REQUIRE(files != nullptr);
  REQUIRE(files->write_all("resource-a.json", std::string_view{R"({"width":300})"}).succeeded());

  const caors::resources::source_location location =
      caors::assets::project_builder{assets}.provider().find(test_guid_a);
  REQUIRE(location);
  CHECK(location.location.string_view() == std::string_view{"caors://assets/resource-a.json"});

  auto loaded =
      assets.create_group(caors::tasks::inline_executor(), caors::tasks::inline_executor())
          .request_load<serialized_resource>(test_guid_a)
          .result();
  REQUIRE(loaded.succeeded());
  REQUIRE(loaded.value != nullptr);
  CHECK(loaded.value->width == 300);
}

TEST_CASE("resources - asset_manager accepts external file system factory", "[resources]") {
  require_serialized_resource_metadata();
  require_typed_external_file_system_config_schema();

  std::string json{R"({
    "file_systems": [
      {
        "name": "memory",
        "type": "external_memory",
        "options": {
          "label": "editor"
        }
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
        "file_system": "vfs"
      }
    ],
    "resource_types": [
      {
        "type": "game.serialized",
        "loader": "json",
        "cache": "memory"
      }
    ],
    "resources": [
      {
        "guid": ")"};
  json += test_guid_a.string();
  json += R"(",
        "source": "assets",
        "location": "caors://assets/resource-a.json",
        "type": "game.serialized"
      }
    ]
  })";

  caors::assets::project_config config;
  REQUIRE(caors::assets::json::load_project_config(config, json).succeeded());

  external_file_system_factory_state state;

  caors::assets::asset_manager assets;
  REQUIRE(caors::assets::project_builder{assets}
              .register_typed_file_system_factory<typed_external_file_system_config,
                                                  &make_typed_external_memory_file_system>(
                  caors::string_name{"external_memory"}, &state)
              .succeeded());
  REQUIRE(caors::assets::json::register_type_factory<serialized_resource>(
              caors::assets::project_builder{assets}, serialized_manifest_type())
              .succeeded());
  REQUIRE(caors::assets::project_builder{assets}.build(config).succeeded());

  CHECK(state.calls == 1);
  CHECK(state.resource == assets.resource());
  CHECK(state.label == "editor");
  REQUIRE(state.file_system != nullptr);
  REQUIRE(state.file_system->write_all("resource-a.json", std::string_view{R"({"width":301})"})
              .succeeded());

  auto loaded =
      assets.create_group(caors::tasks::inline_executor(), caors::tasks::inline_executor())
          .request_load<serialized_resource>(test_guid_a)
          .result();
  REQUIRE(loaded.succeeded());
  REQUIRE(loaded.value != nullptr);
  CHECK(loaded.value->width == 301);
}

TEST_CASE("resources - asset_manager routes source through resolver and mount file systems",
          "[resources]") {
  require_serialized_resource_metadata();

  std::string json{R"({
    "file_systems": [
      {
        "name": "memory",
        "type": "memory"
      },
      {
        "name": "mounted",
        "type": "mount",
        "routes": [
          {
            "prefix": "/assets",
            "target": "memory",
            "target_prefix": "pack"
          }
        ]
      },
      {
        "name": "vfs",
        "type": "resolver",
        "routes": [
          {
            "prefix": "caors://game",
            "target": "mounted"
          }
        ]
      }
    ],
    "sources": [
      {
        "name": "assets",
        "file_system": "vfs"
      }
    ],
    "resource_types": [
      {
        "type": "game.serialized",
        "loader": "json",
        "cache": "memory"
      }
    ],
    "resources": [
      {
        "guid": ")"};
  json += test_guid_a.string();
  json += R"(",
        "source": "assets",
        "location": "caors://game/assets/resource-a.json",
        "type": "game.serialized"
      }
    ]
  })";

  caors::assets::project_config config;
  REQUIRE(caors::assets::json::load_project_config(config, json).succeeded());

  caors::assets::asset_manager assets;
  REQUIRE(caors::assets::json::register_type_factory<serialized_resource>(
              caors::assets::project_builder{assets}, serialized_manifest_type())
              .succeeded());
  REQUIRE(caors::assets::project_builder{assets}.build(config).succeeded());

  auto* memory = dynamic_cast<caors::io::memory_file_system*>(
      caors::assets::project_builder{assets}.find_file_system(caors::string_name{"memory"}));
  REQUIRE(memory != nullptr);
  REQUIRE(
      memory->write_all("pack/resource-a.json", std::string_view{R"({"width":302})"}).succeeded());

  auto* vfs = caors::assets::project_builder{assets}.find_file_system(caors::string_name{"vfs"});
  REQUIRE(vfs != nullptr);
  CHECK(vfs->exists("caors://game/assets/resource-a.json"));

  auto loaded =
      assets.create_group(caors::tasks::inline_executor(), caors::tasks::inline_executor())
          .request_load<serialized_resource>(test_guid_a)
          .result();
  REQUIRE(loaded.succeeded());
  REQUIRE(loaded.value != nullptr);
  CHECK(loaded.value->width == 302);
}

TEST_CASE("resources - asset_manager creates configured source cache and loader from factories",
          "[resources]") {
  require_serialized_resource_metadata();

  caors::assets::project_config config;
  REQUIRE(caors::assets::json::load_project_config(config, make_project_config_json()).succeeded());

  caors::assets::asset_manager assets;
  REQUIRE(caors::assets::json::register_type_factory<serialized_resource>(
              caors::assets::project_builder{assets}, serialized_manifest_type())
              .succeeded());
  REQUIRE(caors::assets::project_builder{assets}.build(config).succeeded());

  auto* source = dynamic_cast<caors::resources::file_source*>(
      caors::assets::project_builder{assets}.find_source(caors::string_name{"assets"}));
  REQUIRE(source != nullptr);

  auto* files = dynamic_cast<caors::io::memory_file_system*>(&source->file_system());
  REQUIRE(files != nullptr);
  REQUIRE(
      files->write_all("assets/resource-a.json", std::string_view{R"({"width":512})"}).succeeded());
  REQUIRE(files->write_all("assets/resource-b.json", std::string_view{R"({"width":1024})"})
              .succeeded());

  auto loaded =
      assets.create_group(caors::tasks::inline_executor(), caors::tasks::inline_executor())
          .request_preload(test_guid_b)
          .result();
  REQUIRE(loaded.succeeded());
  CHECK(loaded.loaded.size() == 2);

  const serialized_resource* resource_a = assets.database().find<serialized_resource>(test_guid_a);
  const serialized_resource* resource_b = assets.database().find<serialized_resource>(test_guid_b);
  REQUIRE(resource_a != nullptr);
  REQUIRE(resource_b != nullptr);
  CHECK(resource_a->width == 512);
  CHECK(resource_b->width == 1024);
}

TEST_CASE("resources - asset_manager builds multiple sources and resource types", "[resources]") {
  require_serialized_resource_metadata();
  require_secondary_serialized_resource_metadata();

  caors::assets::project_config config;
  REQUIRE(caors::assets::json::load_project_config(config, make_multi_source_type_config_json())
              .succeeded());

  caors::assets::asset_manager assets;
  REQUIRE(caors::assets::json::register_type_factory<serialized_resource>(
              caors::assets::project_builder{assets}, serialized_manifest_type())
              .succeeded());
  REQUIRE(caors::assets::json::register_type_factory<secondary_serialized_resource>(
              caors::assets::project_builder{assets}, secondary_manifest_type())
              .succeeded());

  REQUIRE(caors::assets::project_builder{assets}.build(config).succeeded());
  REQUIRE(caors::assets::project_builder{assets}.source_count() == 2);

  auto* base_source = dynamic_cast<caors::resources::file_source*>(
      caors::assets::project_builder{assets}.find_source(caors::string_name{"assets"}));
  auto* dlc_source = dynamic_cast<caors::resources::file_source*>(
      caors::assets::project_builder{assets}.find_source(caors::string_name{"dlc"}));
  REQUIRE(base_source != nullptr);
  REQUIRE(dlc_source != nullptr);
  CHECK(base_source != dlc_source);

  auto* base_files = dynamic_cast<caors::io::memory_file_system*>(&base_source->file_system());
  auto* dlc_files = dynamic_cast<caors::io::memory_file_system*>(&dlc_source->file_system());
  REQUIRE(base_files != nullptr);
  REQUIRE(dlc_files != nullptr);
  REQUIRE(base_files->write_all("base/resource-a.json", std::string_view{R"({"width":64})"})
              .succeeded());
  REQUIRE(dlc_files->write_all("packs/resource-c.json", std::string_view{R"({"width":128})"})
              .succeeded());

  auto base = assets.create_group(caors::tasks::inline_executor(), caors::tasks::inline_executor())
                  .request_load<serialized_resource>(test_guid_a)
                  .result();
  auto dlc = assets.create_group(caors::tasks::inline_executor(), caors::tasks::inline_executor())
                 .request_load<secondary_serialized_resource>(test_guid_c)
                 .result();

  REQUIRE(base.succeeded());
  REQUIRE(dlc.succeeded());
  REQUIRE(base.value != nullptr);
  REQUIRE(dlc.value != nullptr);
  CHECK(base.value->width == 64);
  CHECK(dlc.value->width == 128);
  CHECK(caors::assets::project_builder{assets}.provider().find(test_guid_a).source == base_source);
  CHECK(caors::assets::project_builder{assets}.provider().find(test_guid_c).source == dlc_source);
}

TEST_CASE("resources - asset_manager exposes high level resource operations", "[resources]") {
  require_serialized_resource_metadata();

  caors::assets::project_config config;
  REQUIRE(caors::assets::json::load_project_config(config, make_project_config_json()).succeeded());

  caors::assets::asset_manager assets;
  REQUIRE((caors::assets::register_cached_type_factory<
               serialized_resource, caors::assets::json::sync_loader<serialized_resource>>(
               caors::assets::project_builder{assets}, serialized_manifest_type(),
               caors::string_name{"memory"}, caors::string_name{"json"})
               .succeeded()));
  REQUIRE(caors::assets::project_builder{assets}.build(config).succeeded());

  auto* source = dynamic_cast<caors::resources::file_source*>(
      caors::assets::project_builder{assets}.find_source(caors::string_name{"assets"}));
  REQUIRE(source != nullptr);

  auto* files = dynamic_cast<caors::io::memory_file_system*>(&source->file_system());
  REQUIRE(files != nullptr);
  REQUIRE(
      files->write_all("assets/resource-a.json", std::string_view{R"({"width":512})"}).succeeded());
  REQUIRE(files->write_all("assets/resource-b.json", std::string_view{R"({"width":1024})"})
              .succeeded());

  auto loaded_request =
      assets.create_group(caors::tasks::inline_executor(), caors::tasks::inline_executor())
          .request_load<serialized_resource>(test_guid_a);
  caors::result<serialized_resource*> loaded{loaded_request.error(),
                                             loaded_request.try_get().get()};
  REQUIRE(loaded.succeeded());
  REQUIRE(loaded.value != nullptr);
  CHECK(loaded.value->width == 512);
  CHECK(assets.find<serialized_resource>(test_guid_a) == loaded.value);
  CHECK(assets.find(test_guid_a) == loaded.value);

  const caors::resources::resource_ref_resolver_ops resolver =
      caors::assets::make_resource_resolver(assets);
  CHECK(resolver(caors::resources::make_resource_external_ref(
            test_guid_a, caors::meta::id_of<serialized_resource>())) == loaded.value);

  caors::resource_ref<serialized_resource> reference{test_guid_a};
  REQUIRE(assets.resolve(reference));
  CHECK(reference.get() == loaded.value);
  CHECK(assets.is_current(reference));

  REQUIRE(files->write_all("assets/resource-a.json", std::string_view{R"({"width":2048})"})
              .succeeded());
  auto reloaded_request =
      assets.create_group(caors::tasks::inline_executor(), caors::tasks::inline_executor())
          .request_reload<serialized_resource>(test_guid_a);
  caors::result<serialized_resource*> reloaded{reloaded_request.error(),
                                               reloaded_request.try_get().get()};
  REQUIRE(reloaded.succeeded());
  REQUIRE(reloaded.value != nullptr);
  CHECK(reloaded.value->width == 2048);
  CHECK_FALSE(assets.is_current(reference));
  REQUIRE(assets.resolve(reference));
  CHECK(reference.get() == reloaded.value);

  reloaded.value->width = 4096;
  caors::assets::json::saver<serialized_resource> saver{
      caors::assets::project_builder{assets}.provider()};
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
              caors::assets::project_builder{assets}, serialized_manifest_type())
              .succeeded());
  REQUIRE(caors::assets::project_builder{assets}.build(config).succeeded());

  auto* source = dynamic_cast<caors::resources::file_source*>(
      caors::assets::project_builder{assets}.find_source(caors::string_name{"assets"}));
  REQUIRE(source != nullptr);

  auto* files = dynamic_cast<caors::io::memory_file_system*>(&source->file_system());
  REQUIRE(files != nullptr);
  REQUIRE(
      files->write_all("assets/resource-a.json", std::string_view{R"({"width":512})"}).succeeded());
  REQUIRE(files->write_all("assets/resource-b.json", std::string_view{R"({"width":1024})"})
              .succeeded());

  caors::tasks::executor executor = caors::tasks::inline_executor();
  caors::tasks::execution_context completions;

  caors::resources::load_request<serialized_resource> loaded =
      assets.create_group(executor, completions.get_executor())
          .request_load<serialized_resource>(test_guid_a);
  REQUIRE(loaded);
  CHECK(loaded.status() == caors::resources::request_status::loading);
  CHECK(completions.drain() == 1);
  REQUIRE(loaded.loaded());
  REQUIRE(loaded.try_get());
  CHECK(loaded.try_get()->width == 512);

  REQUIRE(
      files->write_all("assets/resource-a.json", std::string_view{R"({"width":256})"}).succeeded());
  caors::resources::load_request<serialized_resource> reloaded =
      assets.create_group(executor, completions.get_executor())
          .request_reload<serialized_resource>(test_guid_a);
  REQUIRE(reloaded);
  CHECK(reloaded.status() == caors::resources::request_status::loading);
  CHECK(completions.drain() == 1);
  REQUIRE(reloaded.loaded());
  CHECK(reloaded.try_get()->width == 256);

  caors::resources::preload_request preloaded =
      assets.create_group(executor, completions.get_executor()).request_preload(test_guid_b);
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

  caors::assets::asset_manager assets;
  REQUIRE(caors::assets::project_builder{assets}
              .register_source_factory(caors::string_name{"external_memory"},
                                       caors::assets::project_builder::source_factory{
                                           &make_external_memory_source, &state})
              .succeeded());
  REQUIRE(caors::assets::project_builder{assets}
              .register_source_factory(caors::string_name{"external_memory"},
                                       caors::assets::project_builder::source_factory{
                                           &make_external_memory_source, &state})
              .succeeded());
  CHECK(caors::assets::project_builder{assets}.register_source_factory(
            caors::string_name{"external_memory"},
            caors::assets::project_builder::source_factory{&make_external_memory_source,
                                                           &other_state}) ==
        caors::error{caors::error_domain::resources, caors::errc::already_exists});

  REQUIRE(caors::assets::project_builder{assets}.build(config).succeeded());
  CHECK(state.calls == 1);
  CHECK(state.resource == assets.resource());
  CHECK(state.mount == "debug-assets");
  CHECK(state.read_only);
  REQUIRE(state.file_system != nullptr);
  REQUIRE(state.source != nullptr);
  CHECK(caors::assets::project_builder{assets}.find_source(caors::string_name{"assets"}) ==
        state.source);
}

TEST_CASE("resources - asset_manager source factory can use typed options schema", "[resources]") {
  require_typed_external_source_config_schema();

  caors::assets::project_config config;
  REQUIRE(
      caors::assets::json::load_project_config(
          config,
          R"({"sources":[{"name":"assets","type":"typed_external_memory","options":{"mount":"debug-assets","read_only":true,"priority":5,"filter":"linear"}}]})")
          .succeeded());

  external_source_factory_state state;
  caors::assets::asset_manager assets;
  REQUIRE(caors::assets::project_builder{assets}
              .register_typed_source_factory<typed_external_source_config,
                                             &make_typed_external_memory_source>(
                  caors::string_name{"typed_external_memory"}, &state)
              .succeeded());

  REQUIRE(caors::assets::project_builder{assets}.build(config).succeeded());
  CHECK(state.calls == 1);
  CHECK(state.mount == "debug-assets");
  CHECK(state.read_only);
  CHECK(state.priority == 5);
  CHECK(state.filter == "linear");
  REQUIRE(state.source != nullptr);
  CHECK(caors::assets::project_builder{assets}.find_source(caors::string_name{"assets"}) ==
        state.source);

  typed_external_source_config invalid_options;
  invalid_options.mount = "";
  invalid_options.priority = 99;
  invalid_options.filter = "cubic";

  const caors::serialization::config_validation_result validation =
      caors::serialization::validate_config(invalid_options);
  CHECK(validation.status ==
        caors::error{caors::error_domain::serialization, caors::errc::invalid_argument});
  CHECK(validation.diagnostics.size() >= 3);
  CHECK(contains_reason(validation.diagnostics, "有效值"));
  CHECK(contains_reason(validation.diagnostics, "范围"));
  CHECK(contains_reason(validation.diagnostics, "允许值"));
}

TEST_CASE("resources - typed factory options apply defaults and reject unknown members",
          "[resources]") {
  require_typed_external_source_config_schema();

  caors::assets::project_config config;
  REQUIRE(
      caors::assets::json::load_project_config(
          config,
          R"({"sources":[{"name":"assets","type":"typed_external_memory","options":{"mount":"debug-assets","extra":true}}]})")
          .succeeded());

  external_source_factory_state state;
  caors::assets::asset_manager assets;
  REQUIRE(caors::assets::project_builder{assets}
              .register_typed_source_factory<typed_external_source_config,
                                             &make_typed_external_memory_source>(
                  caors::string_name{"typed_external_memory"}, &state)
              .succeeded());

  caors::assets::asset_build_result result =
      caors::assets::project_builder{assets}.build_detailed(config);
  CHECK(result.status ==
        caors::error{caors::error_domain::serialization, caors::errc::invalid_field});
  CHECK(result.stage == caors::assets::asset_build_stage::source);
  REQUIRE_FALSE(result.reasons.empty());
  CHECK(contains_reason(result.reasons, "extra"));
  CHECK(state.calls == 0);

  config.sources[0].options.mutable_object().pop_back();
  state = {};
  REQUIRE(caors::assets::project_builder{assets}.build(config).succeeded());
  CHECK(state.calls == 1);
  CHECK(state.mount == "debug-assets");
  CHECK(state.priority == 3);
  CHECK(state.filter == "nearest");
}

TEST_CASE("resources - asset_manager typed source factory reports option diagnostics",
          "[resources]") {
  require_typed_external_source_config_schema();

  caors::assets::project_config config;
  REQUIRE(
      caors::assets::json::load_project_config(
          config,
          R"({"sources":[{"name":"assets","type":"typed_external_memory","options":{"mount":"","priority":99,"filter":"cubic","extra":true}}]})")
          .succeeded());

  external_source_factory_state state;
  caors::assets::asset_manager assets;
  REQUIRE(caors::assets::project_builder{assets}
              .register_typed_source_factory<typed_external_source_config,
                                             &make_typed_external_memory_source>(
                  caors::string_name{"typed_external_memory"}, &state)
              .succeeded());

  const caors::assets::asset_build_result result =
      caors::assets::project_builder{assets}.build_detailed(config);
  CHECK(result.status ==
        caors::error{caors::error_domain::serialization, caors::errc::invalid_field});
  CHECK(result.stage == caors::assets::asset_build_stage::source);
  CHECK(state.calls == 0);
  REQUIRE_FALSE(result.reasons.empty());
  CHECK(contains_reason(result.reasons, "extra"));
  CHECK(contains_reason(result.reasons, "有效值"));
  CHECK(contains_reason(result.reasons, "范围"));
  CHECK(contains_reason(result.reasons, "允许值"));
}

TEST_CASE("resources - asset_manager typed type factory receives validated options",
          "[resources]") {
  require_serialized_resource_metadata();
  require_typed_external_type_config_schema();

  caors::assets::project_config config;
  REQUIRE(caors::assets::json::load_project_config(config, make_project_config_json()).succeeded());
  config.resource_types[0].cache = caors::string_name{"external_memory"};
  config.resource_types[0].loader = caors::string_name{"external_json"};

  external_type_factory_state state;
  caors::assets::asset_manager assets;
  REQUIRE(caors::assets::project_builder{assets}
              .register_typed_type_factory<typed_external_type_config,
                                           &make_typed_external_serialized_type>(
                  serialized_manifest_type(), caors::string_name{"external_memory"},
                  caors::string_name{"external_json"}, &state)
              .succeeded());

  REQUIRE(caors::assets::project_builder{assets}.build(config).succeeded());
  CHECK(state.calls == 1);
  CHECK(state.format == "resource-json");
  CHECK(state.priority == 2);
  REQUIRE(state.cache != nullptr);
  REQUIRE(state.loader != nullptr);
}

TEST_CASE("resources - asset_manager typed type factory reports option diagnostics",
          "[resources]") {
  require_serialized_resource_metadata();
  require_typed_external_type_config_schema();

  caors::assets::project_config config;
  REQUIRE(
      caors::assets::json::load_project_config(
          config,
          R"({"resource_types":[{"type":"game.serialized","loader":"external_json","cache":"external_memory","options":{"format":"bad","priority":200,"extra":true}}]})")
          .succeeded());

  external_type_factory_state state;
  caors::assets::asset_manager assets;
  REQUIRE(caors::assets::project_builder{assets}
              .register_typed_type_factory<typed_external_type_config,
                                           &make_typed_external_serialized_type>(
                  serialized_manifest_type(), caors::string_name{"external_memory"},
                  caors::string_name{"external_json"}, &state)
              .succeeded());

  const caors::assets::asset_build_result result =
      caors::assets::project_builder{assets}.build_detailed(config);
  CHECK(result.status ==
        caors::error{caors::error_domain::serialization, caors::errc::invalid_field});
  CHECK(result.stage == caors::assets::asset_build_stage::resource_type);
  CHECK(state.calls == 0);
  REQUIRE_FALSE(result.reasons.empty());
  CHECK(contains_reason(result.reasons, "extra"));
  CHECK(contains_reason(result.reasons, "范围"));
  CHECK(contains_reason(result.reasons, "允许值"));
}

TEST_CASE("resources - asset_manager accepts external type factory state", "[resources]") {
  require_serialized_resource_metadata();

  caors::assets::project_config config;
  REQUIRE(caors::assets::json::load_project_config(config, make_project_config_json()).succeeded());
  config.resource_types[0].cache = caors::string_name{"external_memory"};
  config.resource_types[0].loader = caors::string_name{"external_json"};

  external_type_factory_state state;

  caors::assets::asset_manager assets;
  REQUIRE(caors::assets::project_builder{assets}
              .register_type_factory(serialized_manifest_type(),
                                     caors::assets::project_builder::type_factory{
                                         &make_external_serialized_type, &state})
              .succeeded());
  REQUIRE(caors::assets::project_builder{assets}.build(config).succeeded());

  CHECK(state.calls == 1);
  CHECK(state.format == "resource-json");
  CHECK(state.priority == 2);
  REQUIRE(state.cache != nullptr);
  REQUIRE(state.loader != nullptr);

  auto* source = dynamic_cast<caors::resources::file_source*>(
      caors::assets::project_builder{assets}.find_source(caors::string_name{"assets"}));
  REQUIRE(source != nullptr);

  auto* files = dynamic_cast<caors::io::memory_file_system*>(&source->file_system());
  REQUIRE(files != nullptr);
  REQUIRE(
      files->write_all("assets/resource-a.json", std::string_view{R"({"width":128})"}).succeeded());
  REQUIRE(
      files->write_all("assets/resource-b.json", std::string_view{R"({"width":256})"}).succeeded());

  caors::resources::preload_result loaded =
      assets.create_group(caors::tasks::inline_executor(), caors::tasks::inline_executor())
          .request_preload(test_guid_b)
          .result();
  REQUIRE(loaded.succeeded());
  CHECK(loaded.loaded.size() == 2);
  REQUIRE(state.cache->find(test_guid_a) != nullptr);
  REQUIRE(state.cache->find(test_guid_b) != nullptr);
  CHECK(state.cache->find(test_guid_a)->width == 128);
  CHECK(state.cache->find(test_guid_b)->width == 256);
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
    REQUIRE(caors::assets::project_builder{assets}
                .register_source_factory(
                    caors::string_name{"counted"},
                    caors::assets::project_builder::source_factory{&make_counted_source, &state})
                .succeeded());

    REQUIRE(caors::assets::project_builder{assets}.build(config).succeeded());
    CHECK(state.calls == 1);
    CHECK(state.destroyed == 0);
    REQUIRE(caors::assets::project_builder{assets}.find_source(caors::string_name{"assets"}) !=
            nullptr);

    REQUIRE(caors::assets::project_builder{assets}.build(config).succeeded());
    CHECK(state.calls == 2);
    CHECK(state.destroyed == 1);
    REQUIRE(caors::assets::project_builder{assets}.find_source(caors::string_name{"assets"}) !=
            nullptr);
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
  caors::assets::json::sync_loader<serialized_resource> loader{
      caors::assets::project_builder{assets}.provider()};
  REQUIRE(caors::assets::project_builder{assets}
              .bind_source(caors::string_name{"assets"}, source)
              .succeeded());
  REQUIRE(assets.register_type(cache, loader, serialized_manifest_type()).succeeded());

  REQUIRE(caors::assets::project_builder{assets}.build(config).succeeded());
  caors::resources::preload_result loaded =
      assets.create_group(caors::tasks::inline_executor(), caors::tasks::inline_executor())
          .request_preload(test_guid_b)
          .result();
  REQUIRE(loaded.succeeded());
  REQUIRE(cache.find(test_guid_a) != nullptr);

  REQUIRE(caors::assets::project_builder{assets}.build(config).succeeded());
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
              caors::assets::project_builder{assets}, serialized_manifest_type())
              .succeeded());

  const caors::assets::asset_build_result result =
      caors::assets::project_builder{assets}.build_detailed(config);
  CHECK(result.status == caors::error{caors::error_domain::resources, caors::errc::unsupported});
  CHECK(result.stage == caors::assets::asset_build_stage::resource_type);
  CHECK(result.index == 0);
  CHECK(result.name == serialized_manifest_type());
  REQUIRE_FALSE(result.reasons.empty());
}

TEST_CASE("resources - generic cached type factory requires matching cache and loader strategies",
          "[resources]") {
  require_serialized_resource_metadata();

  caors::assets::project_config config;
  REQUIRE(caors::assets::json::load_project_config(config, make_project_config_json()).succeeded());
  config.resource_types[0].cache = caors::string_name{"external_memory"};

  caors::assets::asset_manager assets;
  REQUIRE((caors::assets::register_cached_type_factory<
               serialized_resource, caors::assets::json::sync_loader<serialized_resource>>(
               caors::assets::project_builder{assets}, serialized_manifest_type(),
               caors::string_name{"memory"}, caors::string_name{"json"})
               .succeeded()));

  const caors::assets::asset_build_result result =
      caors::assets::project_builder{assets}.build_detailed(config);
  CHECK(result.status == caors::error{caors::error_domain::resources, caors::errc::unsupported});
  CHECK(result.stage == caors::assets::asset_build_stage::resource_type);
  CHECK(result.name == serialized_manifest_type());
}

TEST_CASE("resources - asset_manager reports missing configured sources", "[resources]") {
  caors::assets::project_config config;
  REQUIRE(caors::assets::json::load_project_config(config, make_project_config_json()).succeeded());
  config.sources[0].type = caors::string_name{"unknown_source"};

  caors::assets::asset_manager assets;
  const caors::assets::asset_build_result result =
      caors::assets::project_builder{assets}.build_detailed(config);
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

#if CAORS_HAS_EXCEPTIONS
namespace {
struct project_allocation_probe {
  caors::memory::tracking_resource upstream;
  bool fail = false;
  int alive = 0;
};
struct owned_registration_probe {
  explicit owned_registration_probe(project_allocation_probe& state) : state(state) {
    ++state.alive;
    // 对象已经构造，下一次拥有对象表扩容失败。
    state.fail = true;
  }
  ~owned_registration_probe() { --state.alive; }
  project_allocation_probe& state;
};
} // namespace

TEST_CASE("assets - project ownership registration failure releases constructed object",
          "[assets][memory][project]") {
  project_allocation_probe probe;
  caors::memory::callback_memory_resource resource{
      [](std::size_t size, std::size_t alignment, void* value) -> void* {
        auto& state = *static_cast<project_allocation_probe*>(value);
        return state.fail ? nullptr : state.upstream.try_allocate(size, alignment);
      },
      [](void* ptr, std::size_t size, std::size_t alignment, void* value) {
        static_cast<project_allocation_probe*>(value)->upstream.deallocate(ptr, size, alignment);
      },
      &probe};
  {
    caors::assets::asset_manager assets{&resource};
    const auto before = probe.upstream.statistics().bytes_in_use;
    REQUIRE_THROWS_AS(
        caors::assets::project_builder{assets}.create_owned<owned_registration_probe>(probe),
        std::bad_alloc);
    CHECK(probe.alive == 0);
    CHECK(probe.upstream.statistics().bytes_in_use == before);
    probe.fail = false;
  }
  CHECK(probe.upstream.statistics().bytes_in_use == 0);
}
#endif

namespace {
template <typename T>
concept exposes_project_binding = requires(T& value, caors::io::file_system& fs) {
  value.bind_file_system(caors::string_name{"files"}, fs);
};
template <typename T>
concept exposes_project_creation = requires(T& value) { value.template create_owned<int>(); };
static_assert(!exposes_project_binding<caors::assets::asset_manager>);
static_assert(!exposes_project_creation<caors::assets::asset_manager>);
static_assert(exposes_project_binding<caors::assets::project_builder>);
static_assert(exposes_project_creation<caors::assets::project_builder>);
} // namespace

namespace {
template <typename T>
concept exposes_provider = requires(T& value) { value.provider(); };
static_assert(!exposes_provider<caors::resources::database>);
static_assert(!exposes_provider<caors::assets::asset_manager>);
static_assert(exposes_provider<caors::assets::project_builder>);
} // namespace

TEST_CASE("assets - default JSON factory freezes source and rejects rebuild while pending",
          "[assets][json-factory]") {
  require_serialized_resource_metadata();
  caors::assets::project_config config;
  REQUIRE_FALSE(caors::assets::json::load_project_config(config, make_project_config_json()));
  caors::tasks::execution_context worker;
  caors::tasks::execution_context completion;
  caors::assets::asset_manager assets{{worker.get_executor(), completion.get_executor()}};
  caors::assets::project_builder project{assets};
  REQUIRE_FALSE(caors::assets::json::register_type_factory<serialized_resource>(
      project, serialized_manifest_type()));
  REQUIRE_FALSE(project.build(config));
  auto* source = dynamic_cast<caors::resources::file_source*>(
      project.find_source(caors::string_name{"assets"}));
  REQUIRE(source);
  REQUIRE_FALSE(source->write("assets/resource-a.json", R"({"width":123})"));
  REQUIRE_FALSE(source->write("assets/resource-b.json", R"({"width":456})"));
  auto request = assets.request_load<serialized_resource>(test_guid_a);
  REQUIRE_FALSE(request.done());
  CHECK(project.build(config).value() == caors::errc::frozen);
  REQUIRE_FALSE(project.provider().rebind(test_guid_a, *source, "assets/resource-b.json"));
  for (int i = 0; i < 32 && !request.done(); ++i) {
    (void)worker.drain();
    (void)completion.drain();
  }
  REQUIRE(request.loaded());
  CHECK(request.try_get()->width == 123);
  auto refresh = assets.request_reload<serialized_resource>(test_guid_a);
  for (int i = 0; i < 32 && !refresh.done(); ++i) {
    (void)worker.drain();
    (void)completion.drain();
  }
  REQUIRE(refresh.loaded());
  CHECK(refresh.try_get()->width == 456);
  CHECK_FALSE(assets.database().has_active_operations());
  CHECK_FALSE(project.build(config));
  CHECK(request.try_get()->width == 123);
}

TEST_CASE("assets - JSON factory requires explicit worker mode for native files",
          "[assets][json-factory]") {
  require_serialized_resource_metadata();
  const auto mode = GENERATE(caors::resources::file_read_mode::native_async,
                             caors::resources::file_read_mode::worker_adapter);
  const auto directory = make_temp_dir();
  std::error_code ec;
  std::filesystem::create_directories(directory, ec);
  REQUIRE_FALSE(ec);
  const auto location = path_string(directory / "json-factory.json");
  auto files = std::make_shared<caors::io::native_file_system>();
  REQUIRE_FALSE(files->write_all(location, R"({"width":789})"));
  caors::resources::file_source source{files};
  caors::assets::project_config config;
  REQUIRE_FALSE(caors::assets::json::load_project_config(config, make_project_config_json()));
  caors::tasks::execution_context worker;
  caors::tasks::execution_context completion;
  caors::assets::asset_manager assets{{worker.get_executor(), completion.get_executor()}};
  caors::assets::project_builder project{assets};
  REQUIRE_FALSE(caors::assets::json::register_type_factory<serialized_resource>(
      project, serialized_manifest_type(), mode));
  REQUIRE_FALSE(project.build(config));
  REQUIRE_FALSE(project.provider().rebind(test_guid_a, source, location));
  auto request = assets.request_load<serialized_resource>(test_guid_a);
  for (int i = 0; i < 32 && !request.done(); ++i) {
    (void)worker.drain();
    (void)completion.drain();
  }
  if (mode == caors::resources::file_read_mode::native_async) {
    REQUIRE(request.failed());
    CHECK(request.error().value() == caors::errc::unsupported);
  } else {
    REQUIRE(request.loaded());
    CHECK(request.try_get()->width == 789);
  }
  CHECK_FALSE(assets.database().has_active_operations());
  std::filesystem::remove(location, ec);
  CHECK_FALSE(ec);
}
