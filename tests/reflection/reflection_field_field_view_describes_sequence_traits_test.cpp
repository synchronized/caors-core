// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "reflection_test_support.h"

using namespace test_reflection;

TEST_CASE("reflection - field view describes sequence traits", "[reflection]") {
  const caors::reflection::type reflected = require_field_traits_owner_reflection();

  const caors::reflection::field sequence = require_field(reflected, "sequence");
  CHECK(sequence.kind() == caors::meta::type_kind::sequence);
  CHECK(sequence.value_type() == registered_type_id("int"));

  const caors::reflection::field span = require_field(reflected, "span");
  CHECK(span.kind() == caors::meta::type_kind::sequence);
  CHECK(span.value_type() == registered_type_id("int"));
}
