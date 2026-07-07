// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - json saver saves resource through provider", "[resources]") {
  require_serialized_resource_metadata();

  caors::memory_file_system files;
  caors::resource_file_source source{files};
  caors::resource_provider provider;
  REQUIRE(provider.bind(test_guid_a, source, "assets/resource.json").succeeded());

  serialized_resource value{test_guid_a};
  value.width = 640;

  caors::resource_json_saver<serialized_resource> saver{provider};
  REQUIRE(saver.save(value).succeeded());

  caors::byte_buffer_result bytes = files.read_all("assets/resource.json");
  REQUIRE(bytes.succeeded());
  CHECK(std::string_view{reinterpret_cast<const char*>(bytes.value.data()), bytes.value.size()} ==
        R"({"width":640})");

  caors::resource_json_loader<serialized_resource> loader{provider};
  caors::resource_cache<serialized_resource> cache;
  caors::result<serialized_resource*> loaded = caors::load_into(cache, loader, test_guid_a);

  REQUIRE(loaded.succeeded());
  REQUIRE(loaded.value != nullptr);
  CHECK(loaded.value->guid() == test_guid_a);
  CHECK(loaded.value->width == 640);
}
