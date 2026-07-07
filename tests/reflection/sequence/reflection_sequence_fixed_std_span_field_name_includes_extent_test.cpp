// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "reflection_test_support.h"

#include "caors/value/associative_entry_ref.h"
#include "caors/value/view_factory.h"

using namespace test_reflection;

TEST_CASE("reflection - fixed std span field name includes extent", "[reflection]") {
  REQUIRE(caors::reflection::class_<fixed_span_holder>("caors.test.reflection.fixed_span_holder")
              .field("values", &fixed_span_holder::values)
              .commit()
              .succeeded());

  auto reflected = caors::reflection::find("caors.test.reflection.fixed_span_holder");
  REQUIRE(reflected);

  auto values = reflected.find_field("values");
  REQUIRE(values);
  CHECK(values.kind() == caors::meta::type_kind::sequence);
  CHECK(values.type_name() == "std.span<int,3>");
  CHECK(values.element_type() == registered_type_id("int"));
}
