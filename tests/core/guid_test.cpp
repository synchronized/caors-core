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

TEST_CASE("core - guid accepts uppercase text and formats lowercase", "[core]") {
  const auto parsed = caors::guid::parse("2F6D1F64-7A3A-4DB2-9A2D-123456789ABC");

  REQUIRE(parsed.has_value());
  CHECK(parsed->string() == "2f6d1f64-7a3a-4db2-9a2d-123456789abc");
}

TEST_CASE("core - guid rejects invalid text", "[core]") {
  CHECK_FALSE(caors::guid::parse("").has_value());
  CHECK_FALSE(caors::guid::parse("2f6d1f64-7a3a-4db2-9a2d-123456789ab").has_value());
  CHECK_FALSE(caors::guid::parse("2f6d1f64_7a3a_4db2_9a2d_123456789abc").has_value());
  CHECK_FALSE(caors::guid::parse("2f6d1f64-7a3a-4db2-9a2d-123456789abx").has_value());
}

TEST_CASE("core - default guid is nil", "[core]") {
  constexpr caors::guid value = caors::guid::nil();

  CHECK_FALSE(value);
  CHECK(value.is_nil());
  CHECK(value.string() == "00000000-0000-0000-0000-000000000000");
}
