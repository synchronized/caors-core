// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include <caors/meta/query.h>
#include <caors/meta/type.h>
#include <caors/meta/type_creation.h>

#include <catch2/catch_all.hpp>

TEST_CASE("headers - meta type query and creation headers expose type facade",
          "[headers]") {
  caors::meta::type empty{};
  CHECK_FALSE(empty.valid());

  const caors::meta::type integer = caors::meta::resolve<int>();
  REQUIRE(integer.valid());
  CHECK_FALSE(caors::meta::find<int>().valid());
  CHECK(integer.size() == sizeof(int));
  CHECK(integer.alignment() == alignof(int));

  caors::meta::any value = integer.create_value();
  REQUIRE(value.valid());
  REQUIRE(value.try_get<int>() != nullptr);
  CHECK(*value.try_get<int>() == 0);
}
