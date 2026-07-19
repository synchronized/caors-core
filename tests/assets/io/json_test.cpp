// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "../../resources/resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - json loader loads resource through provider", "[resources]") {
  require_serialized_resource_metadata();

  const std::filesystem::path directory = make_temp_dir();
  const std::filesystem::path file = directory / "resource.json";
  std::error_code ec;
  std::filesystem::remove_all(directory, ec);
  REQUIRE(std::filesystem::create_directories(directory, ec));
  REQUIRE_FALSE(ec);

  caors::io::native_file_system native;
  const std::string file_path = path_string(file);
  REQUIRE(native.write_all(file_path, std::string_view{R"({"width":512})"}).succeeded());

  caors::resources::file_source source{native};
  caors::resources::provider provider;
  REQUIRE(provider.bind(test_guid_a, source, file_path).succeeded());

  caors::assets::json::loader<serialized_resource> loader{provider};
  caors::resource_cache<serialized_resource> cache;

  caors::resources::handle_result<serialized_resource> loaded =
      caors::resources::load_handle_into(cache, loader, test_guid_a);

  REQUIRE(loaded.succeeded());
  REQUIRE(loaded.value);
  CHECK(loaded.value->guid() == test_guid_a);
  CHECK(loaded.value->width == 512);
  CHECK(cache.find(test_guid_a) == loaded.value.get());

  std::filesystem::remove_all(directory, ec);
}

#include "../../resources/resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - json loader reports parse failures", "[resources]") {
  const std::filesystem::path directory = make_temp_dir();
  const std::filesystem::path file = directory / "invalid.json";
  std::error_code ec;
  std::filesystem::remove_all(directory, ec);
  REQUIRE(std::filesystem::create_directories(directory, ec));
  REQUIRE_FALSE(ec);

  caors::io::native_file_system native;
  const std::string file_path = path_string(file);
  REQUIRE(native.write_all(file_path, std::string_view{R"({"width":)"}).succeeded());

  caors::resources::file_source source{native};
  caors::resources::provider provider;
  REQUIRE(provider.bind(test_guid_a, source, file_path).succeeded());

  caors::assets::json::loader<serialized_resource> loader{provider};
  caors::resources::load_result<serialized_resource> loaded = loader.load(test_guid_a);

  REQUIRE(loaded.failed());
  CHECK(loaded.status.domain() == caors::error_domain::serialization);
  CHECK(loaded.status.value() == caors::errc::invalid_argument);

  std::filesystem::remove_all(directory, ec);
}

#include "../../resources/resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - json saver reports invalid and missing targets", "[resources]") {
  require_serialized_resource_metadata();

  caors::io::memory_file_system files;
  caors::resources::file_source source{files};
  caors::resources::provider provider;
  REQUIRE(provider.bind(test_guid_a, source, "assets/resource.json").succeeded());

  caors::assets::json::saver<serialized_resource> saver{provider};

  serialized_resource invalid_resource{};
  caors::error invalid = saver.save(invalid_resource);
  REQUIRE(invalid.failed());
  CHECK(invalid.domain() == caors::error_domain::resources);
  CHECK(invalid.value() == caors::errc::invalid_argument);

  serialized_resource missing_resource{test_guid_b};
  missing_resource.width = 7;
  caors::error missing = saver.save(missing_resource);
  REQUIRE(missing.failed());
  CHECK(missing.domain() == caors::error_domain::resources);
  CHECK(missing.value() == caors::errc::not_found);
}

#include "../../resources/resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - json saver saves resource through provider", "[resources]") {
  require_serialized_resource_metadata();

  caors::io::memory_file_system files;
  caors::resources::file_source source{files};
  caors::resources::provider provider;
  REQUIRE(provider.bind(test_guid_a, source, "assets/resource.json").succeeded());

  serialized_resource value{test_guid_a};
  value.width = 640;

  caors::assets::json::saver<serialized_resource> saver{provider};
  REQUIRE(saver.save(value).succeeded());

  caors::io::byte_buffer_result bytes = files.read_all("assets/resource.json");
  REQUIRE(bytes.succeeded());
  CHECK(std::string_view{reinterpret_cast<const char*>(bytes.value.data()), bytes.value.size()} ==
        R"({"width":640})");

  caors::assets::json::loader<serialized_resource> loader{provider};
  caors::resource_cache<serialized_resource> cache;
  caors::resources::handle_result<serialized_resource> loaded =
      caors::resources::load_handle_into(cache, loader, test_guid_a);

  REQUIRE(loaded.succeeded());
  REQUIRE(loaded.value);
  CHECK(loaded.value->guid() == test_guid_a);
  CHECK(loaded.value->width == 640);
}

TEST_CASE("assets - project config can load through format agnostic loader", "[assets]") {
  caors::io::memory_file_system files;
  REQUIRE(files
              .write_all("project.caors", std::string_view{R"({
    "schema_version": 1,
    "sources": [
      {
        "name": "assets",
        "type": "memory_file_system",
        "root": "assets"
      }
    ],
    "resource_types": [
      {
        "type": "caors.test.resources.serialized_resource",
        "loader": "json",
        "cache": "memory"
      }
    ],
    "resources": [
      {
        "guid": "6cd2da3d-b549-4756-8b8b-0c84293af410",
        "source": "assets",
        "location": "resource.json",
        "type": "caors.test.resources.serialized_resource",
        "hash": "hash",
        "version": 2
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
  })"})
              .succeeded());

  caors::resources::file_source source{files};
  caors::assets::project_config config;
  caors::assets::json::project_config_loader loader;

  REQUIRE(caors::assets::load_project_config(config, loader, source, "project.caors").succeeded());

  REQUIRE(config.sources.size() == 1);
  CHECK(config.sources[0].name == caors::string_name{"assets"});
  CHECK(config.sources[0].type == caors::string_name{"memory_file_system"});
  CHECK(config.sources[0].root.string_view() == "assets");

  REQUIRE(config.resource_types.size() == 1);
  CHECK(config.resource_types[0].type ==
        caors::string_name{"caors.test.resources.serialized_resource"});
  CHECK(config.resource_types[0].loader == caors::string_name{"json"});
  CHECK(config.resource_types[0].cache == caors::string_name{"memory"});

  REQUIRE(config.resources.size() == 1);
  CHECK(config.resources[0].guid == test_guid_a);
  CHECK(config.resources[0].source == caors::string_name{"assets"});
  CHECK(config.resources[0].location.string_view() == "resource.json");
  CHECK(config.resources[0].hash == "hash");
  CHECK(config.resources[0].version == 2);

  REQUIRE(config.dependencies.size() == 1);
  CHECK(config.dependencies[0].resource == test_guid_b);
  CHECK(config.dependencies[0].dependency == test_guid_a);

  REQUIRE(config.preload_roots.size() == 1);
  CHECK(config.preload_roots[0] == test_guid_b);
}
