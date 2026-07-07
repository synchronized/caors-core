// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - provider writes bytes through file source", "[resources]") {
  caors::memory_file_system files;
  caors::resource_file_source source{files};
  caors::resource_provider provider;

  REQUIRE(provider.bind(test_guid_a, source, "assets/generated.txt").succeeded());
  REQUIRE(provider.write(test_guid_a, std::string_view{"generated-bytes"}).succeeded());

  caors::byte_buffer_result bytes = files.read_all("assets/generated.txt");
  REQUIRE(bytes.succeeded());
  CHECK(std::string_view{reinterpret_cast<const char*>(bytes.value.data()), bytes.value.size()} ==
        "generated-bytes");

  REQUIRE(provider.write(test_guid_a, std::string_view{"updated-bytes"}).succeeded());
  bytes = provider.read(test_guid_a);
  REQUIRE(bytes.succeeded());
  CHECK(std::string_view{reinterpret_cast<const char*>(bytes.value.data()), bytes.value.size()} ==
        "updated-bytes");
}
