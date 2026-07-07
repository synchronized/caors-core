// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - json loader reports parse failures", "[resources]") {
  const std::filesystem::path directory = make_temp_dir();
  const std::filesystem::path file = directory / "invalid.json";
  std::error_code ec;
  std::filesystem::remove_all(directory, ec);
  REQUIRE(std::filesystem::create_directories(directory, ec));
  REQUIRE_FALSE(ec);

  caors::native_file_system native;
  const std::string file_path = path_string(file);
  REQUIRE(native.write_all(file_path, std::string_view{R"({"width":)"}).succeeded());

  caors::resource_file_source source{native};
  caors::resource_provider provider;
  REQUIRE(provider.bind(test_guid_a, source, file_path).succeeded());

  caors::resource_json_loader<serialized_resource> loader{provider};
  caors::resource_load_result<serialized_resource> loaded = loader.load(test_guid_a);

  REQUIRE(loaded.failed());
  CHECK(loaded.status.domain() == caors::error_domain::serialization);
  CHECK(loaded.status.value() == caors::errc::invalid_argument);

  std::filesystem::remove_all(directory, ec);
}
