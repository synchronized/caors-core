// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/core/guid.h"

#include <catch2/catch_all.hpp>

TEST_CASE("core - guid accepts uppercase text and formats lowercase", "[core]") {
  const auto parsed = caors::guid::parse("2F6D1F64-7A3A-4DB2-9A2D-123456789ABC");

  REQUIRE(parsed.has_value());
  CHECK(parsed->string() == "2f6d1f64-7a3a-4db2-9a2d-123456789abc");
}

TEST_CASE("core - guid hash preserves 64 bit result and folds 32 bit result", "[core]") {
  const auto value = caors::guid::from_bytes(
      caors::guid::byte_array{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16});
  constexpr auto expected =
      static_cast<std::size_t>(sizeof(std::size_t) == 8 ? 0xa069b9bf124863c3ULL : 0xb221da7cULL);
  CHECK(caors::guid_hash{}(value) == expected);
}
