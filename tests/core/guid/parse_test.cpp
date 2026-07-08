// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/core/guid.h"

#include <catch2/catch_all.hpp>

TEST_CASE("core - guid parses canonical text", "[core]") {
  const auto parsed = caors::guid::parse("2f6d1f64-7a3a-4db2-9a2d-123456789abc");

  REQUIRE(parsed.has_value());
  CHECK(*parsed);
  CHECK_FALSE(parsed->is_nil());
  CHECK(parsed->string() == "2f6d1f64-7a3a-4db2-9a2d-123456789abc");
}
