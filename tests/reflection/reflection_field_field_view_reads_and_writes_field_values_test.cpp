// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "reflection_test_support.h"

using namespace test_reflection;

TEST_CASE("reflection - field view reads and writes field values", "[reflection]") {
  REQUIRE(caors::reflection::class_<read_write_actor>("caors.test.reflection.read_write_actor")
              .field("id", &read_write_actor::id)
              .field("health", &read_write_actor::health)
              .commit() == caors::error{});

  const caors::reflection::type reflected =
      require_reflected("caors.test.reflection.read_write_actor");
  const caors::reflection::field id = require_field(reflected, "id");
  const caors::reflection::field health = require_field(reflected, "health");

  read_write_actor instance{1, 25.0F};

  auto id_value = id.try_get<int>(&instance);
  REQUIRE(id_value.has_value());
  CHECK(*id_value == 1);

  CHECK(id.try_set<int>(&instance, 42));
  CHECK(instance.id == 42);

  auto health_value = health.try_get<float>(&instance);
  REQUIRE(health_value.has_value());
  CHECK(*health_value == 25.0F);

  CHECK(health.try_set<float>(&instance, 75.0F));
  CHECK(instance.health == 75.0F);

  CHECK_FALSE(id.try_get<float>(&instance).has_value());
  CHECK_FALSE(id.try_get<int>(nullptr).has_value());
  CHECK_FALSE(id.try_set<float>(&instance, 1.0F));
  CHECK_FALSE(id.try_set<int>(nullptr, 1));
}
