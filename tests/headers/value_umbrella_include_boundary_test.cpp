// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include <caors/value.h>

#include <catch2/catch_all.hpp>

TEST_CASE("headers - value umbrella exposes complete value module", "[headers]") {
  caors::any value = caors::any::from(7);
  REQUIRE(value.valid());

  caors::any_ref ref = value.ref();
  REQUIRE(ref.valid());
  CHECK(ref.matches_type(value));

  caors::value::sequence_view sequence{};
  CHECK_FALSE(sequence.valid());

  caors::variant data = caors::variant::from(true);
  CHECK(data.is_bool());
  CHECK(data.as_bool());
}
