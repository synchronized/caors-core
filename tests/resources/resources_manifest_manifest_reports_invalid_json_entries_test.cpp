// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - manifest reports invalid json entries", "[resources]") {
  caors::resource_manifest manifest;

  caors::error invalid_guid = caors::load_manifest_json(
      manifest, R"({"resources":[{"guid":"not-a-guid","location":"asset.json"}]})");
  REQUIRE(invalid_guid.failed());
  CHECK(invalid_guid.domain() == caors::error_domain::resources);
  CHECK(invalid_guid.value() == caors::errc::invalid_argument);

  caors::error missing_entries = caors::load_manifest_json(manifest, R"({"items":[]})");
  REQUIRE(missing_entries.failed());
  CHECK(missing_entries.domain() == caors::error_domain::resources);
  CHECK(missing_entries.value() == caors::errc::invalid_argument);

  caors::error syntax_error = caors::load_manifest_json(manifest, R"({"resources":)");
  REQUIRE(syntax_error.failed());
  CHECK(syntax_error.domain() == caors::error_domain::serialization);
  CHECK(syntax_error.value() == caors::errc::invalid_argument);
}
