// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - json saver reports invalid and missing targets", "[resources]") {
  require_serialized_resource_metadata();

  caors::memory_file_system files;
  caors::resource_file_source source{files};
  caors::resource_provider provider;
  REQUIRE(provider.bind(test_guid_a, source, "assets/resource.json").succeeded());

  caors::resource_json_saver<serialized_resource> saver{provider};

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
