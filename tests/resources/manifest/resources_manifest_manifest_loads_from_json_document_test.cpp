// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - manifest loads from json document", "[resources]") {
  caors::resource_manifest manifest;
  const std::string json =
      std::string{R"({"resources":[{"guid":")"} + test_guid_a.string() +
      R"(","location":"/assets//texture.json","type":"game.texture","hash":"etag-a","version":9}]})";

  REQUIRE(caors::load_manifest_json(manifest, json).succeeded());

  const caors::resource_manifest_entry* entry = manifest.find(test_guid_a);
  REQUIRE(entry != nullptr);
  CHECK(entry->location.string_view() == "assets/texture.json");
  CHECK(entry->type == std::string_view{"game.texture"});
  CHECK(std::string_view{entry->hash.data(), entry->hash.size()} == "etag-a");
  CHECK(entry->version == 9);

  const std::string updated_json =
      std::string{R"([{"guid":")"} + test_guid_a.string() +
      R"(","location":"assets/texture-v2.json","hash":"etag-b","version":10}])";
  REQUIRE(caors::load_manifest_json(manifest, updated_json).succeeded());

  CHECK(manifest.size() == 1);
  entry = manifest.find(test_guid_a);
  REQUIRE(entry != nullptr);
  CHECK(entry->location.string_view() == "assets/texture-v2.json");
  CHECK(std::string_view{entry->hash.data(), entry->hash.size()} == "etag-b");
  CHECK(entry->version == 10);
}
