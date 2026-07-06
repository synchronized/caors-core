// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "reflection_test_support.h"

using namespace test_reflection;

TEST_CASE("reflection - type view stores and finds fields", "[reflection]") {
  REQUIRE(
      caors::reflection::class_<stored_fields_actor>("caors.test.reflection.stored_fields_actor")
          .field("id", &stored_fields_actor::id)
          .field("health", &stored_fields_actor::health)
          .commit() == caors::error{});

  const caors::reflection::type type =
      require_reflected("caors.test.reflection.stored_fields_actor");

  CHECK(type.valid());
  CHECK(type.field_count() == 2);

  auto health = type.find_field("health");
  REQUIRE(health);
  CHECK(health.type_name() == "float");
  CHECK_FALSE(type.find_field("missing"));
}
