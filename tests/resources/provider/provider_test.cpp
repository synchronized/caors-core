// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：resources_provider_provider_reads_bytes_from_file_source_test.cpp

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

// 原测试文件：resources_provider_provider_reports_invalid_and_missing_resources_test.cpp

#include "resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - provider reports invalid and missing resources", "[resources]") {
  caors::native_file_system native;
  caors::resource_file_source source{native};
  caors::resource_provider provider;

  caors::error invalid_guid = provider.bind(caors::resource_guid::nil(), source, "asset.txt");
  REQUIRE(invalid_guid.failed());
  CHECK(invalid_guid.domain() == caors::error_domain::resources);
  CHECK(invalid_guid.value() == caors::errc::invalid_argument);

  caors::error invalid_write =
      provider.write(caors::resource_guid::nil(), std::string_view{"data"});
  REQUIRE(invalid_write.failed());
  CHECK(invalid_write.domain() == caors::error_domain::resources);
  CHECK(invalid_write.value() == caors::errc::invalid_argument);

  caors::error invalid_location = provider.bind(test_guid_a, source, "");
  REQUIRE(invalid_location.failed());
  CHECK(invalid_location.domain() == caors::error_domain::resources);
  CHECK(invalid_location.value() == caors::errc::invalid_argument);

  REQUIRE(provider.bind(test_guid_a, source, "missing.txt").succeeded());
  caors::error duplicate = provider.bind(test_guid_a, source, "other.txt");
  REQUIRE(duplicate.failed());
  CHECK(duplicate.domain() == caors::error_domain::resources);
  CHECK(duplicate.value() == caors::errc::already_exists);

  caors::byte_buffer_result missing_guid = provider.read(test_guid_b);
  REQUIRE(missing_guid.failed());
  CHECK(missing_guid.status.domain() == caors::error_domain::resources);
  CHECK(missing_guid.status.value() == caors::errc::not_found);

  caors::error missing_write = provider.write(test_guid_b, std::string_view{"data"});
  REQUIRE(missing_write.failed());
  CHECK(missing_write.domain() == caors::error_domain::resources);
  CHECK(missing_write.value() == caors::errc::not_found);

  caors::byte_buffer_result missing_file = provider.read(test_guid_a);
  REQUIRE(missing_file.failed());
  CHECK(missing_file.status.domain() == caors::error_domain::io);
  CHECK(missing_file.status.value() == caors::errc::not_found);
}

// 原测试文件：resources_provider_provider_reports_unsupported_writes_for_read_only_sources_test.cpp

#include "resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - provider reports unsupported writes for read only sources", "[resources]") {
  read_only_source source;
  caors::resource_provider provider;

  REQUIRE(provider.bind(test_guid_a, source, "assets/generated.txt").succeeded());

  caors::error status = provider.write(test_guid_a, std::string_view{"generated-bytes"});
  REQUIRE(status.failed());
  CHECK(status.domain() == caors::error_domain::resources);
  CHECK(status.value() == caors::errc::unsupported);
}

// 原测试文件：resources_provider_provider_writes_bytes_through_file_source_test.cpp

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
