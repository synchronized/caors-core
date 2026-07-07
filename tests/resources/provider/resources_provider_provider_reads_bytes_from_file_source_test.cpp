// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - provider reads bytes from file source", "[resources]") {
  const std::filesystem::path directory = make_temp_dir();
  const std::filesystem::path file = directory / "texture.txt";
  std::error_code ec;
  std::filesystem::remove_all(directory, ec);
  REQUIRE(std::filesystem::create_directories(directory, ec));
  REQUIRE_FALSE(ec);

  caors::native_file_system native;
  const std::string file_path = path_string(file);
  REQUIRE(native.write_all(file_path, std::string_view{"texture-bytes"}).succeeded());

  caors::resource_file_source source{native};
  caors::resource_provider provider;

  REQUIRE(provider.bind(test_guid_a, source, file_path).succeeded());
  CHECK(provider.size() == 1);
  CHECK(provider.contains(test_guid_a));
  CHECK(provider.exists(test_guid_a));

  const caors::resource_source_location location = provider.find(test_guid_a);
  REQUIRE(location);
  CHECK(location.source == &source);
  CHECK(location.location.string_view() == file_path);

  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  caors::byte_buffer_result bytes = provider.read(test_guid_a, &tracking);

  REQUIRE(bytes.succeeded());
  CHECK(bytes.value.get_allocator().resource() == &tracking);
  CHECK(std::string_view{reinterpret_cast<const char*>(bytes.value.data()), bytes.value.size()} ==
        "texture-bytes");

  std::filesystem::remove_all(directory, ec);
}
