// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - manifest stores normalized locations and metadata", "[resources]") {
  caors::resource_manifest manifest;
  const caors::string_name texture_type{"game.texture"};

  REQUIRE(
      manifest.bind(test_guid_a, "/assets//texture.json", texture_type, "etag-a", 7).succeeded());

  CHECK(manifest.size() == 1);
  CHECK(manifest.contains(test_guid_a));

  const caors::resource_manifest_entry* entry = manifest.find(test_guid_a);
  REQUIRE(entry != nullptr);
  CHECK(entry->guid == test_guid_a);
  CHECK(entry->location.string_view() == "assets/texture.json");
  CHECK(entry->type == std::string_view{"game.texture"});
  CHECK(std::string_view{entry->hash.data(), entry->hash.size()} == "etag-a");
  CHECK(entry->version == 7);
  CHECK(entry->valid());

  const caors::error duplicate = manifest.bind(test_guid_a, "other.json");
  REQUIRE(duplicate.failed());
  CHECK(duplicate.domain() == caors::error_domain::resources);
  CHECK(duplicate.value() == caors::errc::already_exists);

  REQUIRE(
      manifest.set(test_guid_a, "assets/texture-v2.json", texture_type, "etag-b", 8).succeeded());
  CHECK(manifest.size() == 1);
  entry = manifest.find(test_guid_a);
  REQUIRE(entry != nullptr);
  CHECK(entry->location.string_view() == "assets/texture-v2.json");
  CHECK(std::string_view{entry->hash.data(), entry->hash.size()} == "etag-b");
  CHECK(entry->version == 8);

  const caors::error invalid_guid = manifest.bind(caors::resource_guid::nil(), "asset.json");
  REQUIRE(invalid_guid.failed());
  CHECK(invalid_guid.domain() == caors::error_domain::resources);
  CHECK(invalid_guid.value() == caors::errc::invalid_argument);

  const caors::error empty_location = manifest.bind(test_guid_b, "///");
  REQUIRE(empty_location.failed());
  CHECK(empty_location.domain() == caors::error_domain::resources);
  CHECK(empty_location.value() == caors::errc::invalid_argument);
}
