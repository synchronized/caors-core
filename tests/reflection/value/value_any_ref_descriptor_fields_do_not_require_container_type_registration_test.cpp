// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "value_test_common.h"

using namespace caors::tests::reflection;

TEST_CASE("reflection any - descriptor fields do not require container type registration",
          "[reflection]") {
  require_value_test_meta();

  REQUIRE(caors::reflection::class_<unregistered_nested_vector_holder>(
              "caors.test.reflection.unregistered_nested_vector_holder")
              .field("values", &unregistered_nested_vector_holder::values)
              .commit() == caors::error{});

  auto reflected =
      caors::reflection::find("caors.test.reflection.unregistered_nested_vector_holder");
  REQUIRE(reflected);
  auto values_field = reflected.find_field("values");
  REQUIRE(values_field);
  CHECK(values_field.type_name() == "std.vector<std.vector<int>>");
  CHECK(values_field.size() == sizeof(std::vector<std::vector<int>>));
  CHECK(values_field.alignment() == alignof(std::vector<std::vector<int>>));
  CHECK(values_field.kind() == caors::meta::type_kind::sequence);
  CHECK(static_cast<bool>(values_field.value_type()));
  CHECK_FALSE(caors::meta::find(values_field.value_type()));

  unregistered_nested_vector_holder holder{{{1, 2}, {3}}};
  caors::value::sequence_view outer = values_field.get_ref(&holder).create_sequence_view();
  REQUIRE(outer.valid());
  REQUIRE(outer.size().has_value());
  CHECK(*outer.size() == 2);

  caors::value::sequence_view inner = outer.element_ref(0).create_sequence_view();
  REQUIRE(inner.valid());
  CHECK(inner.element_type() == caors::any::from(0).type());
  CHECK(inner.element_ref(0).try_set(5));
  CHECK(holder.values[0][0] == 5);
}
