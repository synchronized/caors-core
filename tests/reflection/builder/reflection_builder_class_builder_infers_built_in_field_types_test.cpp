// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "reflection_test_support.h"

using namespace test_reflection;

TEST_CASE("reflection - class builder infers built-in field types", "[reflection]") {
  const caors::error result =
      caors::reflection::class_<builder_actor>("caors.test.reflection.builder_actor")
          .field("id", &builder_actor::id)
          .field("health", &builder_actor::health)
          .commit();

  REQUIRE(result == caors::error{});

  auto reflected = caors::reflection::find("caors.test.reflection.builder_actor");
  REQUIRE(reflected);

  auto id = reflected.find_field("id");
  REQUIRE(id);
  CHECK(id.type_name() == "int");
  CHECK(id.type() == registered_type_id("int"));

  auto health = reflected.find_field("health");
  REQUIRE(health);
  CHECK(health.type_name() == "float");
  CHECK(health.type() == registered_type_id("float"));

  builder_actor instance{3, 80.0F};
  CHECK(id.try_set<int>(&instance, 10));
  CHECK(health.try_set<float>(&instance, 20.0F));
  CHECK(instance.id == 10);
  CHECK(instance.health == 20.0F);
}
