// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - manifest can populate provider bindings", "[resources]") {
  caors::memory_file_system files;
  REQUIRE(files.write_all("assets/texture.json", std::string_view{"texture-bytes"}).succeeded());

  caors::resource_file_source source{files};
  caors::resource_manifest manifest;
  REQUIRE(manifest.bind(test_guid_a, "/assets//texture.json").succeeded());

  caors::resource_provider provider;
  REQUIRE(caors::bind_manifest(provider, source, manifest).succeeded());

  CHECK(provider.size() == 1);
  CHECK(provider.exists(test_guid_a));

  const caors::resource_source_location location = provider.find(test_guid_a);
  REQUIRE(location);
  CHECK(location.source == &source);
  CHECK(location.location.string_view() == "assets/texture.json");

  const caors::byte_buffer_result bytes = provider.read(test_guid_a);
  REQUIRE(bytes.succeeded());
  CHECK(std::string_view{reinterpret_cast<const char*>(bytes.value.data()), bytes.value.size()} ==
        "texture-bytes");

  REQUIRE(files.write_all("assets/texture-v2.json", std::string_view{"updated-bytes"}).succeeded());
  REQUIRE(manifest.set(test_guid_a, "/assets//texture-v2.json").succeeded());
  REQUIRE(caors::sync_manifest(provider, source, manifest).succeeded());

  CHECK(provider.size() == 1);
  const caors::resource_source_location updated_location = provider.find(test_guid_a);
  REQUIRE(updated_location);
  CHECK(updated_location.location.string_view() == "assets/texture-v2.json");

  const caors::byte_buffer_result updated_bytes = provider.read(test_guid_a);
  REQUIRE(updated_bytes.succeeded());
  CHECK(std::string_view{reinterpret_cast<const char*>(updated_bytes.value.data()),
                         updated_bytes.value.size()} == "updated-bytes");
}
