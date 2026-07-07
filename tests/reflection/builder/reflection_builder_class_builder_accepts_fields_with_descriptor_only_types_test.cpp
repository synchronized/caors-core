// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "reflection_test_support.h"

using namespace test_reflection;

TEST_CASE("reflection - class builder accepts fields with descriptor-only types", "[reflection]") {
  struct unregistered_field {};
  struct owner {
    unregistered_field field{};
  };

  REQUIRE(caors::reflection::class_<owner>("caors.test.reflection.owner")
              .field("field", &owner::field)
              .commit() == caors::error{});

  auto reflected = caors::reflection::find("caors.test.reflection.owner");
  REQUIRE(reflected);

  auto field = reflected.find_field("field");
  REQUIRE(field);
  CHECK(field.type_name().empty());
  CHECK_FALSE(caors::meta::find(field.type()));
  CHECK(field.value_type() == field.type());
  CHECK_FALSE(field.indirect());

  owner instance{};
  CHECK(field.try_get<unregistered_field>(&instance).has_value());
}
