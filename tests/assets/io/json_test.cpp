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
