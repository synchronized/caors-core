// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - manifest loads array json from source", "[resources]") {
  const std::string json =
      std::string{R"([{"guid":")"} + test_guid_b.string() + R"(","location":"assets/mesh.json"}])";

  caors::memory_file_system files;
  REQUIRE(files.write_all("manifest.json", std::string_view{json}).succeeded());

  caors::resource_file_source source{files};
  caors::resource_manifest manifest;

  REQUIRE(caors::load_manifest(manifest, source, "manifest.json").succeeded());

  const caors::resource_manifest_entry* entry = manifest.find(test_guid_b);
  REQUIRE(entry != nullptr);
  CHECK(entry->location.string_view() == "assets/mesh.json");
  CHECK(entry->type.empty());
  CHECK(entry->hash.empty());
  CHECK(entry->version == 0);
}
