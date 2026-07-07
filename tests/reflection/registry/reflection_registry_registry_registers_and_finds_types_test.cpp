// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "reflection_registry_test_support.h"

using namespace test_reflection;

TEST_CASE("reflection - registry registers and finds types", "[reflection]") {
  REQUIRE(register_registry_actor() == caors::error{});

  CHECK(caors::reflection::size() >= 1);

  auto by_name = caors::reflection::find("caors.test.reflection.registry_actor");
  REQUIRE(by_name);
  CHECK(caors::reflection::contains(by_name.id()));
  CHECK(by_name.field_count() == 2);

  auto by_guid = caors::reflection::find(registry_actor_guid);
  REQUIRE(by_guid);
  CHECK(by_guid.id() == by_name.id());
}
