// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - manifest can replace provider bindings", "[resources]") {
  caors::memory_file_system files;
  REQUIRE(files.write_all("assets/texture.json", std::string_view{"texture-bytes"}).succeeded());
  REQUIRE(files.write_all("assets/old.json", std::string_view{"old-bytes"}).succeeded());
  REQUIRE(files.write_all("assets/new.json", std::string_view{"new-bytes"}).succeeded());

  caors::resource_file_source source{files};
  caors::resource_provider provider;

  caors::resource_manifest initial_manifest;
  REQUIRE(initial_manifest.bind(test_guid_a, "assets/texture.json").succeeded());
  REQUIRE(caors::bind_manifest(provider, source, initial_manifest).succeeded());
  REQUIRE(provider.bind(test_guid_b, source, "assets/old.json").succeeded());

  caors::resource_manifest replacement_manifest;
  REQUIRE(replacement_manifest.bind(test_guid_b, "assets/new.json").succeeded());
  REQUIRE(caors::replace_manifest(provider, source, replacement_manifest).succeeded());

  CHECK(provider.size() == 1);
  CHECK_FALSE(provider.contains(test_guid_a));
  CHECK(provider.contains(test_guid_b));

  const caors::resource_source_location location = provider.find(test_guid_b);
  REQUIRE(location);
  CHECK(location.source == &source);
  CHECK(location.location.string_view() == "assets/new.json");

  const caors::byte_buffer_result bytes = provider.read(test_guid_b);
  REQUIRE(bytes.succeeded());
  CHECK(std::string_view{reinterpret_cast<const char*>(bytes.value.data()), bytes.value.size()} ==
        "new-bytes");
}
