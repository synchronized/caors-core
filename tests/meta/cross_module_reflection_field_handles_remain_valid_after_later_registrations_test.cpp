// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "cross_module_test_support.h"

using namespace caors::tests::cross_module;
TEST_CASE("reflection - field handles remain valid after later registrations", "[reflection]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<cross_module_actor>("caors.test.meta.cross_module_actor")
          .field("id", &cross_module_actor::id)
          .field("health", &cross_module_actor::health)
          .commit()));

  auto reflected = caors::reflection::find("caors.test.meta.cross_module_actor");
  REQUIRE(reflected);

  auto id = reflected.find_field("id");
  REQUIRE(id);
  REQUIRE(id.valid());

  REQUIRE(reflection_result_ok(caors::reflection::class_<cross_module_late_registered>(
                                   "caors.test.meta.cross_module_late_registered")
                                   .field("value", &cross_module_late_registered::value)
                                   .commit()));

  const cross_module_actor instance{7, 12.0F};
  const auto value = id.try_get<int>(&instance);
  REQUIRE(value.has_value());
  CHECK(*value == 7);
  CHECK(id.valid());
}
