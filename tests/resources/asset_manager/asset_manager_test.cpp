// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：resources_asset_manager_project_config_builds_runtime_resource_environment_test.cpp

#include "resources_test_common.h"

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
        "root": "/"
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

} // namespace

TEST_CASE("resources - asset_manager builds provider manifest and dependency graph from config",
          "[resources]") {
  require_serialized_resource_metadata();

  caors::resource_project_config config;
  REQUIRE(caors::load_project_config_json(config, make_project_config_json()).succeeded());
  REQUIRE(config.sources.size() == 1);
  REQUIRE(config.resource_types.size() == 1);
  REQUIRE(config.resources.size() == 2);
  REQUIRE(config.dependencies.size() == 1);
  REQUIRE(config.preload_roots.size() == 2);
  CHECK(config.sources[0].name == std::string_view{"assets"});
  CHECK(config.sources[0].type == std::string_view{"memory_file_system"});
  CHECK(config.resource_types[0].type == serialized_manifest_type());
  CHECK(config.resource_types[0].loader == std::string_view{"json"});
  CHECK(config.resource_types[0].cache == std::string_view{"memory"});
  CHECK(config.preload_roots[0] == test_guid_b);
  CHECK(config.preload_roots[1] == test_guid_c);

  caors::memory_file_system files;
  REQUIRE(files.write_all("config/project.json", make_project_config_json()).succeeded());
  REQUIRE(
      files.write_all("assets/resource-a.json", std::string_view{R"({"width":512})"}).succeeded());
  REQUIRE(
      files.write_all("assets/resource-b.json", std::string_view{R"({"width":1024})"}).succeeded());

  caors::resource_file_source source{files};
  caors::resource_project_config loaded_from_source;
  REQUIRE(
      caors::load_project_config(loaded_from_source, source, "config/project.json").succeeded());
  CHECK(loaded_from_source.resources.size() == 2);

  caors::resource_asset_manager assets;
  REQUIRE(assets.bind_source(caors::string_name{"assets"}, source).succeeded());
  REQUIRE(assets.build(config).succeeded());

  const caors::resource_manifest_entry* entry_a = assets.manifest().find(test_guid_a);
  REQUIRE(entry_a != nullptr);
  CHECK(entry_a->location.string_view() == std::string_view{"assets/resource-a.json"});
  CHECK(entry_a->type == serialized_manifest_type());
  CHECK(std::string_view{entry_a->hash.data(), entry_a->hash.size()} == "hash-a");
  CHECK(entry_a->version == 1);
  CHECK(assets.provider().contains(test_guid_a));
  CHECK(assets.provider().find(test_guid_b).location.string_view() ==
        std::string_view{"assets/resource-b.json"});
  CHECK(assets.dependency_graph().contains(test_guid_b, test_guid_a));

  caors::resource_json_loader<serialized_resource> loader{assets.provider()};
  caors::resource_cache<serialized_resource> cache;
  REQUIRE(assets.register_type(cache, loader, serialized_manifest_type()).succeeded());

  caors::resource_preload_result loaded = assets.preload(test_guid_b);
  REQUIRE(loaded.succeeded());
  REQUIRE(loaded.loaded.size() == 2);
  CHECK(loaded.loaded[0] == caors::resource_preload_item{test_guid_a, serialized_manifest_type()});
  CHECK(loaded.loaded[1] == caors::resource_preload_item{test_guid_b, serialized_manifest_type()});

  const serialized_resource* resource_a = cache.find(test_guid_a);
  const serialized_resource* resource_b = cache.find(test_guid_b);
  REQUIRE(resource_a != nullptr);
  REQUIRE(resource_b != nullptr);
  CHECK(resource_a->width == 512);
  CHECK(resource_b->width == 1024);
}

TEST_CASE("resources - asset_manager reports missing configured sources", "[resources]") {
  caors::resource_project_config config;
  REQUIRE(caors::load_project_config_json(config, make_project_config_json()).succeeded());

  caors::resource_asset_manager assets;
  const caors::error status = assets.build(config);
  CHECK(status == caors::error{caors::error_domain::resources, caors::errc::not_found});
}
