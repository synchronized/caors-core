// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "reflection_test_support.h"

using namespace test_reflection;

TEST_CASE("reflection - field view describes value and enum traits", "[reflection]") {
  const caors::reflection::type reflected = require_field_traits_owner_reflection();

  const caors::reflection::field value = require_field(reflected, "value");
  CHECK(value.kind() == caors::meta::type_kind::value);
  CHECK(value.wrapper_semantics() == caors::meta::wrapper_semantics::none);
  CHECK(value.value_type() == registered_type_id("int"));
  CHECK_FALSE(value.indirect());
  CHECK_FALSE(value.nullable());

  const caors::reflection::field enumeration = require_field(reflected, "color");
  CHECK(enumeration.kind() == caors::meta::type_kind::enumeration);
  CHECK(enumeration.value_type() == registered_type_id("caors.test.reflection.color"));
  CHECK_FALSE(enumeration.nullable());
}
