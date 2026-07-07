// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

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
