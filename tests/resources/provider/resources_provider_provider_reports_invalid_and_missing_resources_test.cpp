// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

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
