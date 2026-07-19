// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：resources_manifest_manifest_can_populate_provider_bindings_test.cpp

#include "resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - manifest can populate provider bindings", "[resources]") {
  caors::io::memory_file_system files;
  REQUIRE(files.write_all("assets/texture.json", std::string_view{"texture-bytes"}).succeeded());

  caors::resources::file_source source{files};
  caors::resource_manifest manifest;
  REQUIRE(manifest.bind(test_guid_a, "/assets//texture.json").succeeded());

  caors::resources::provider provider;
  REQUIRE(caors::resources::bind_manifest(provider, source, manifest).succeeded());

  CHECK(provider.size() == 1);
  CHECK(provider.exists(test_guid_a));

  const caors::resources::source_location location = provider.find(test_guid_a);
  REQUIRE(location);
  CHECK(location.source == &source);
  CHECK(location.location.string_view() == "assets/texture.json");

  const caors::io::byte_buffer_result bytes = provider.read(test_guid_a);
  REQUIRE(bytes.succeeded());
  CHECK(std::string_view{reinterpret_cast<const char*>(bytes.value.data()), bytes.value.size()} ==
        "texture-bytes");

  REQUIRE(files.write_all("assets/texture-v2.json", std::string_view{"updated-bytes"}).succeeded());
  REQUIRE(manifest.set(test_guid_a, "/assets//texture-v2.json").succeeded());
  REQUIRE(caors::resources::sync_manifest(provider, source, manifest).succeeded());

  CHECK(provider.size() == 1);
  const caors::resources::source_location updated_location = provider.find(test_guid_a);
  REQUIRE(updated_location);
  CHECK(updated_location.location.string_view() == "assets/texture-v2.json");

  const caors::io::byte_buffer_result updated_bytes = provider.read(test_guid_a);
  REQUIRE(updated_bytes.succeeded());
  CHECK(std::string_view{reinterpret_cast<const char*>(updated_bytes.value.data()),
                         updated_bytes.value.size()} == "updated-bytes");
}

// 原测试文件：resources_manifest_manifest_can_replace_provider_bindings_test.cpp

#include "resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - manifest can replace provider bindings", "[resources]") {
  caors::io::memory_file_system files;
  REQUIRE(files.write_all("assets/texture.json", std::string_view{"texture-bytes"}).succeeded());
  REQUIRE(files.write_all("assets/old.json", std::string_view{"old-bytes"}).succeeded());
  REQUIRE(files.write_all("assets/new.json", std::string_view{"new-bytes"}).succeeded());

  caors::resources::file_source source{files};
  caors::resources::provider provider;

  caors::resource_manifest initial_manifest;
  REQUIRE(initial_manifest.bind(test_guid_a, "assets/texture.json").succeeded());
  REQUIRE(caors::resources::bind_manifest(provider, source, initial_manifest).succeeded());
  REQUIRE(provider.bind(test_guid_b, source, "assets/old.json").succeeded());

  caors::resource_manifest replacement_manifest;
  REQUIRE(replacement_manifest.bind(test_guid_b, "assets/new.json").succeeded());
  REQUIRE(caors::resources::replace_manifest(provider, source, replacement_manifest).succeeded());

  CHECK(provider.size() == 1);
  CHECK_FALSE(provider.contains(test_guid_a));
  CHECK(provider.contains(test_guid_b));

  const caors::resources::source_location location = provider.find(test_guid_b);
  REQUIRE(location);
  CHECK(location.source == &source);
  CHECK(location.location.string_view() == "assets/new.json");

  const caors::io::byte_buffer_result bytes = provider.read(test_guid_b);
  REQUIRE(bytes.succeeded());
  CHECK(std::string_view{reinterpret_cast<const char*>(bytes.value.data()), bytes.value.size()} ==
        "new-bytes");
}

// 原测试文件：resources_manifest_manifest_loads_array_json_from_source_test.cpp

#include "resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - manifest loads array json from source", "[resources]") {
  const std::string json =
      std::string{R"([{"guid":")"} + test_guid_b.string() + R"(","location":"assets/mesh.json"}])";

  caors::io::memory_file_system files;
  REQUIRE(files.write_all("manifest.json", std::string_view{json}).succeeded());

  caors::resources::file_source source{files};
  caors::resource_manifest manifest;

  REQUIRE(caors::assets::json::load_manifest(manifest, source, "manifest.json").succeeded());

  const caors::resources::manifest_entry* entry = manifest.find(test_guid_b);
  REQUIRE(entry != nullptr);
  CHECK(entry->location.string_view() == "assets/mesh.json");
  CHECK(entry->type.empty());
  CHECK(entry->hash.empty());
  CHECK(entry->version == 0);
}

// 原测试文件：resources_manifest_manifest_loads_from_json_document_test.cpp

#include "resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - manifest loads from json document", "[resources]") {
  caors::resource_manifest manifest;
  const std::string json =
      std::string{R"({"resources":[{"guid":")"} + test_guid_a.string() +
      R"(","location":"/assets//texture.json","type":"game.texture","hash":"etag-a","version":9}]})";

  REQUIRE(caors::assets::json::load_manifest(manifest, json).succeeded());

  const caors::resources::manifest_entry* entry = manifest.find(test_guid_a);
  REQUIRE(entry != nullptr);
  CHECK(entry->location.string_view() == "assets/texture.json");
  CHECK(entry->type == std::string_view{"game.texture"});
  CHECK(std::string_view{entry->hash.data(), entry->hash.size()} == "etag-a");
  CHECK(entry->version == 9);

  const std::string updated_json =
      std::string{R"([{"guid":")"} + test_guid_a.string() +
      R"(","location":"assets/texture-v2.json","hash":"etag-b","version":10}])";
  REQUIRE(caors::assets::json::load_manifest(manifest, updated_json).succeeded());

  CHECK(manifest.size() == 1);
  entry = manifest.find(test_guid_a);
  REQUIRE(entry != nullptr);
  CHECK(entry->location.string_view() == "assets/texture-v2.json");
  CHECK(std::string_view{entry->hash.data(), entry->hash.size()} == "etag-b");
  CHECK(entry->version == 10);
}

// 原测试文件：resources_manifest_manifest_reports_invalid_json_entries_test.cpp

#include "resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - manifest reports invalid json entries", "[resources]") {
  caors::resource_manifest manifest;

  caors::error invalid_guid = caors::assets::json::load_manifest(
      manifest, R"({"resources":[{"guid":"not-a-guid","location":"asset.json"}]})");
  REQUIRE(invalid_guid.failed());
  CHECK(invalid_guid.domain() == caors::error_domain::resources);
  CHECK(invalid_guid.value() == caors::errc::invalid_argument);

  caors::error missing_entries = caors::assets::json::load_manifest(manifest, R"({"items":[]})");
  REQUIRE(missing_entries.failed());
  CHECK(missing_entries.domain() == caors::error_domain::resources);
  CHECK(missing_entries.value() == caors::errc::invalid_argument);

  caors::error syntax_error = caors::assets::json::load_manifest(manifest, R"({"resources":)");
  REQUIRE(syntax_error.failed());
  CHECK(syntax_error.domain() == caors::error_domain::serialization);
  CHECK(syntax_error.value() == caors::errc::invalid_argument);
}

// 原测试文件：resources_manifest_manifest_stores_normalized_locations_and_metadata_test.cpp

#include "resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - manifest stores normalized locations and metadata", "[resources]") {
  caors::resource_manifest manifest;
  const caors::string_name texture_type{"game.texture"};

  REQUIRE(
      manifest.bind(test_guid_a, "/assets//texture.json", texture_type, "etag-a", 7).succeeded());

  CHECK(manifest.size() == 1);
  CHECK(manifest.contains(test_guid_a));

  const caors::resources::manifest_entry* entry = manifest.find(test_guid_a);
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

TEST_CASE("resources - manifest preserves uri locations", "[resources]") {
  caors::resource_manifest manifest;

  REQUIRE(manifest.bind(test_guid_a, "caors://assets//textures\\albedo.json").succeeded());

  const caors::resources::manifest_entry* entry = manifest.find(test_guid_a);
  REQUIRE(entry != nullptr);
  CHECK(entry->location.string_view() == "caors://assets/textures/albedo.json");

  caors::io::memory_file_system storage;
  REQUIRE(storage.write_all("textures/albedo.json", std::string_view{"texture"}).succeeded());

  caors::io::file_system_resolver resolver;
  REQUIRE(resolver.mount("caors://assets", storage).succeeded());

  caors::resources::file_source source{resolver};
  caors::resources::provider provider;
  REQUIRE(caors::resources::bind_manifest(provider, source, manifest).succeeded());

  const caors::resources::source_location location = provider.find(test_guid_a);
  REQUIRE(location);
  CHECK(location.location.string_view() == "caors://assets/textures/albedo.json");

  const caors::io::byte_buffer_result bytes = provider.read(test_guid_a);
  REQUIRE(bytes.succeeded());
  CHECK(std::string_view{reinterpret_cast<const char*>(bytes.value.data()), bytes.value.size()} ==
        "texture");
}
