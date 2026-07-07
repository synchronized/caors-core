// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "reflection_test_support.h"

using namespace test_reflection;

TEST_CASE("reflection - class builder resolves registered C++ field types", "[reflection]") {
  REQUIRE(caors::reflection::class_<inventory>("caors.test.reflection.inventory")
              .field("count", &inventory::count)
              .commit() == caors::error{});

  REQUIRE(caors::reflection::class_<inventory_holder>("caors.test.reflection.inventory_holder")
              .field("items", &inventory_holder::items)
              .commit() == caors::error{});

  auto reflected = caors::reflection::find("caors.test.reflection.inventory_holder");
  REQUIRE(reflected);

  auto items = reflected.find_field("items");
  REQUIRE(items);
  CHECK(items.type_name() == "caors.test.reflection.inventory");
  CHECK(items.type() == registered_type_id("caors.test.reflection.inventory"));
}
