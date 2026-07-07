// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "reflection_test_support.h"

using namespace test_reflection;

TEST_CASE("reflection - field view describes composite traits", "[reflection]") {
  const caors::reflection::type reflected = require_field_traits_owner_reflection();

  const caors::reflection::field pair = require_field(reflected, "pair");
  CHECK(pair.kind() == caors::meta::type_kind::pair);
  CHECK(pair.key_type() == registered_type_id("int"));
  CHECK(pair.value_type() == registered_type_id("caors.string"));

  const caors::reflection::field tuple = require_field(reflected, "tuple");
  CHECK(tuple.kind() == caors::meta::type_kind::tuple);
  CHECK_FALSE(static_cast<bool>(tuple.key_type()));
  CHECK_FALSE(static_cast<bool>(tuple.value_type()));

  const caors::reflection::field variant = require_field(reflected, "variant");
  CHECK(variant.kind() == caors::meta::type_kind::variant);
  CHECK_FALSE(static_cast<bool>(variant.key_type()));
  CHECK_FALSE(static_cast<bool>(variant.value_type()));
}
