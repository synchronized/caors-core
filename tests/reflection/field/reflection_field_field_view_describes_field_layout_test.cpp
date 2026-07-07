// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "reflection_test_support.h"

using namespace test_reflection;

TEST_CASE("reflection - field view describes field layout", "[reflection]") {
  REQUIRE(caors::reflection::class_<field_layout_actor>("caors.test.reflection.field_layout_actor")
              .field("health", &field_layout_actor::health)
              .commit() == caors::error{});

  const caors::reflection::type reflected =
      require_reflected("caors.test.reflection.field_layout_actor");
  const caors::reflection::field field = require_field(reflected, "health");

  CHECK(field.valid());
  CHECK(field.name() == "health");
  CHECK(field.type() == registered_type_id("float"));
  CHECK(field.offset() == offsetof(field_layout_actor, health));
  CHECK(field.writable());
  CHECK(field.type_name() == "float");
  CHECK(field.size() == sizeof(float));
  CHECK(field.alignment() == alignof(float));
}
