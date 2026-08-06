// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "../../resources/resources_test_common.h"

#include "caors/tasks.h"

#include <string>
#include <string_view>

using namespace caors::tests::resources;

namespace {

[[nodiscard]] caors::string_name serialized_manifest_type() {
  return caors::string_name{"game.serialized"};
}

[[nodiscard]] std::string make_vfs_project_config_json() {
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
        "location": "caors://assets/assets/resource-a.json",
        "type": "game.serialized"
      },
      {
        "guid": ")";
  json += test_guid_b.string();
  json += R"(",
        "source": "assets",
        "location": "caors://assets/assets/resource-b.json",
        "type": "game.serialized"
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
  json += R"("
    ]
  })";
  return json;
}

[[nodiscard]] std::string bytes_to_string(const caors::io::byte_buffer& bytes) {
  return std::string{reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

} // namespace

TEST_CASE("assets - project config builds async resource pipeline", "[assets]") {
  require_serialized_resource_metadata();

  caors::assets::project_config config;
  REQUIRE(
      caors::assets::json::load_project_config(config, make_vfs_project_config_json()).succeeded());

  caors::assets::asset_manager assets;
  REQUIRE(caors::assets::json::register_type_factory<serialized_resource>(
              assets, serialized_manifest_type())
              .succeeded());
  REQUIRE(assets.build(config).succeeded());

  auto* files = dynamic_cast<caors::io::memory_file_system*>(
      assets.find_file_system(caors::string_name{"memory"}));
  REQUIRE(files != nullptr);
  REQUIRE(
      files->write_all("assets/resource-a.json", std::string_view{R"({"width":512})"}).succeeded());
  REQUIRE(files->write_all("assets/resource-b.json", std::string_view{R"({"width":1024})"})
              .succeeded());

  const caors::resources::manifest_entry* entry = assets.manifest().find(test_guid_b);
  REQUIRE(entry != nullptr);
  CHECK(entry->type == serialized_manifest_type());
  CHECK(entry->location.string_view() == std::string_view{"caors://assets/assets/resource-b.json"});
  CHECK(assets.provider().contains(test_guid_a));
  CHECK(assets.dependency_graph().contains(test_guid_b, test_guid_a));

  caors::tasks::executor executor = caors::tasks::inline_executor();
  caors::tasks::completion_queue completions;

  caors::resources::load_request<serialized_resource> direct =
      assets.request_load<serialized_resource>(test_guid_a, executor, completions.get_executor());
  caors::resources::preload_request preload =
      assets.request_preload(test_guid_b, executor, completions.get_executor());

  REQUIRE(direct);
  REQUIRE(preload);
  CHECK(direct.status() == caors::resources::request_status::loading);
  CHECK(preload.total_count() == 2);
  CHECK(preload.pending_count() == 2);
  CHECK(completions.size() == 2);

  CHECK(completions.drain() == 2);
  REQUIRE(direct.loaded());
  REQUIRE(preload.loaded());
  CHECK(preload.loaded_count() == 2);
  CHECK(preload.failed_count() == 0);
  REQUIRE(assets.find<serialized_resource>(test_guid_a) != nullptr);
  REQUIRE(assets.find<serialized_resource>(test_guid_b) != nullptr);
  CHECK(assets.find<serialized_resource>(test_guid_a)->width == 512);
  CHECK(assets.find<serialized_resource>(test_guid_b)->width == 1024);

  caors::resource_ref<serialized_resource> reference{test_guid_a};
  REQUIRE(assets.resolve(reference));
  CHECK(reference.get() == assets.find<serialized_resource>(test_guid_a));
  CHECK(assets.is_current(reference));

  REQUIRE(files->write_all("assets/resource-a.json", std::string_view{R"({"width":2048})"})
              .succeeded());
  caors::resources::load_request<serialized_resource> reloaded =
      assets.request_reload<serialized_resource>(test_guid_a, executor, completions.get_executor());
  REQUIRE(reloaded);
  CHECK(completions.drain() == 1);
  REQUIRE(reloaded.loaded());
  REQUIRE(reloaded.try_get());
  CHECK(reloaded.try_get()->width == 2048);
  CHECK_FALSE(assets.is_current(reference));
  REQUIRE(assets.resolve(reference));
  CHECK(reference.get() == reloaded.try_get().get());

  caors::resource_handle<serialized_resource> handle = reloaded.try_get();
  REQUIRE(handle);
  handle->width = 4096;

  caors::assets::json::saver<serialized_resource> saver{assets.provider()};
  REQUIRE(saver.save(*handle).succeeded());

  caors::io::byte_buffer_result saved = files->read_all("assets/resource-a.json");
  REQUIRE(saved.succeeded());
  CHECK(bytes_to_string(saved.value) == R"({"width":4096})");
}
