// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - rejects invalid json", "[serialization]") {
  const caors::serialization::variant_result parsed = caors::parse_json(R"({"id":1,})");

  CHECK(parsed.failed());
  CHECK(parsed.status.domain() == caors::error_domain::serialization);
  CHECK(parsed.status.value() == caors::errc::invalid_argument);
}
