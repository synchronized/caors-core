// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include <caors/meta/value.h>

#include <catch2/catch_all.hpp>

TEST_CASE("headers - meta value umbrella exposes complete value module", "[headers]") {
  caors::meta::any value = caors::meta::any::from(7);
  REQUIRE(value.valid());

  caors::meta::any_ref ref = value.ref();
  REQUIRE(ref.valid());
  CHECK(ref.matches_type(value));

  caors::meta::sequence_view sequence{};
  CHECK_FALSE(sequence.valid());

  caors::meta::variant data = caors::meta::variant::from(true);
  CHECK(data.is_bool());
  CHECK(data.as_bool());
}
