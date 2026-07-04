// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include <caors/value/any.h>

#include <catch2/catch_all.hpp>

TEST_CASE("headers - value any header exposes owning any", "[headers]") {
  caors::any value{};
  CHECK_FALSE(value.valid());

  caors::any stored = caors::any::from(42);
  REQUIRE(stored.valid());
  REQUIRE(stored.try_get<int>() != nullptr);
  CHECK(*stored.try_get<int>() == 42);
}
