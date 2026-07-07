// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/core/guid.h"

#include <catch2/catch_all.hpp>

TEST_CASE("core - guid accepts uppercase text and formats lowercase", "[core]") {
  const auto parsed = caors::guid::parse("2F6D1F64-7A3A-4DB2-9A2D-123456789ABC");

  REQUIRE(parsed.has_value());
  CHECK(parsed->string() == "2f6d1f64-7a3a-4db2-9a2d-123456789abc");
}
