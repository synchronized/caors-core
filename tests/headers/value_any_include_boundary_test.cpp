// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include <caors/meta/value/any.h>

#include <catch2/catch_all.hpp>

TEST_CASE("headers - meta value any header exposes owning any", "[headers]") {
  caors::meta::any value{};
  CHECK_FALSE(value.valid());

  caors::meta::any stored = caors::meta::any::from(42);
  REQUIRE(stored.valid());
  REQUIRE(stored.try_get<int>() != nullptr);
  CHECK(*stored.try_get<int>() == 42);
}
