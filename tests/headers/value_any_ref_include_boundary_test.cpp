// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include <caors/meta/value/any_ref.h>

#include <catch2/catch_all.hpp>

TEST_CASE("headers - meta value any_ref header exposes runtime references", "[headers]") {
  int number = 1;

  caors::meta::any_ref ref = caors::meta::any_ref::from(number);
  REQUIRE(ref.valid());
  REQUIRE(ref.try_get<int>() != nullptr);
  CHECK(*ref.try_get<int>() == 1);

  CHECK(ref.try_set(2));
  CHECK(number == 2);
}
