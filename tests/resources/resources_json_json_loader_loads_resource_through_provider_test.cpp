// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - json loader loads resource through provider", "[resources]") {
  require_serialized_resource_metadata();

  const std::filesystem::path directory = make_temp_dir();
  const std::filesystem::path file = directory / "resource.json";
  std::error_code ec;
  std::filesystem::remove_all(directory, ec);
  REQUIRE(std::filesystem::create_directories(directory, ec));
  REQUIRE_FALSE(ec);

  caors::native_file_system native;
  const std::string file_path = path_string(file);
  REQUIRE(native.write_all(file_path, std::string_view{R"({"width":512})"}).succeeded());

  caors::resource_file_source source{native};
  caors::resource_provider provider;
  REQUIRE(provider.bind(test_guid_a, source, file_path).succeeded());

  caors::resource_json_loader<serialized_resource> loader{provider};
  caors::resource_cache<serialized_resource> cache;

  caors::result<serialized_resource*> loaded = caors::load_into(cache, loader, test_guid_a);

  REQUIRE(loaded.succeeded());
  REQUIRE(loaded.value != nullptr);
  CHECK(loaded.value->guid() == test_guid_a);
  CHECK(loaded.value->width == 512);
  CHECK(cache.find(test_guid_a) == loaded.value);

  std::filesystem::remove_all(directory, ec);
}
