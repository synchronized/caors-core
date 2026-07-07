// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_resource_ref_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::resource_ref_codec;
TEST_CASE("serialization - rejects invalid resource ref guids", "[serialization]") {
  require_resource_ref_actor_metadata();

  resource_ref_actor decoded{};
  const caors::error status = caors::deserialize_json(decoded, R"({"texture":"not-a-guid"})");

  REQUIRE(status.failed());
  CHECK(status.domain() == caors::error_domain::serialization);
  CHECK(status.value() == caors::errc::invalid_argument);
}
