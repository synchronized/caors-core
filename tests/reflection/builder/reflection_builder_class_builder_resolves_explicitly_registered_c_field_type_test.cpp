// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "reflection_test_support.h"

using namespace test_reflection;

TEST_CASE("reflection - class builder resolves explicitly registered C++ field type",
          "[reflection]") {
  require_registered_type<test_reflection::named_field_value>(
      "caors.test.reflection.named_field_value");

  REQUIRE(caors::reflection::class_<test_reflection::named_field_owner>(
              "caors.test.reflection.named_field_owner")
              .field("value", &test_reflection::named_field_owner::value)
              .commit() == caors::error{});

  auto value_type = caors::meta::find("caors.test.reflection.named_field_value");
  REQUIRE(value_type);
  CHECK(value_type.id() == caors::meta::id_of<test_reflection::named_field_value>());

  auto reflected = caors::reflection::find("caors.test.reflection.named_field_owner");
  REQUIRE(reflected);

  auto value = reflected.find_field("value");
  REQUIRE(value);
  CHECK(value.type_name() == "caors.test.reflection.named_field_value");
  CHECK(value.type() == value_type.id());
}
