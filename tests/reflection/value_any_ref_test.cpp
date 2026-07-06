// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "value_test_common.h"

using namespace caors::tests::reflection;

TEST_CASE("reflection any_ref - references external typed values", "[reflection]") {
  require_value_test_meta();

  int source = 7;
  caors::any_ref reference = caors::any_ref::from(source);

  CHECK(reference.valid());
  CHECK(reference.writable());
  CHECK(reference.matches_type<int>());
  REQUIRE(reference.try_get<int>() != nullptr);
  CHECK(*reference.try_get<int>() == 7);

  int* writable = reference.try_get_mutable<int>();
  REQUIRE(writable != nullptr);
  *writable = 11;
  CHECK(source == 11);
  CHECK(reference.try_set(13));
  CHECK(source == 13);

  caors::any copied = caors::any::from(17);
  CHECK(reference.try_set(copied));
  CHECK(source == 17);

  const int readonly = 3;
  caors::any_ref readonly_reference = caors::any_ref::from_const(readonly);
  CHECK(readonly_reference.valid());
  CHECK_FALSE(readonly_reference.writable());
  CHECK(readonly_reference.try_get_mutable<int>() == nullptr);
  CHECK_FALSE(readonly_reference.try_set(4));
  REQUIRE(readonly_reference.try_get<int>() != nullptr);
  CHECK(*readonly_reference.try_get<int>() == 3);

  int target = 0;
  caors::any_ref target_reference = caors::any_ref::from(target);
  CHECK(target_reference.try_set(readonly_reference));
  CHECK(target == 3);
}

TEST_CASE("reflection any_ref - creates descriptor views for unregistered nested vectors",
          "[reflection]") {
  require_value_test_meta();

  std::vector<std::vector<int>> values{{1, 2}, {3}};

  caors::any_ref reference = caors::any_ref::from(values);
  REQUIRE(reference.valid());
  CHECK(reference.type_name().empty());

  caors::value::sequence_view outer = reference.create_sequence_view();
  REQUIRE(outer.valid());
  CHECK(outer.writable());
  CHECK(static_cast<bool>(outer.value_type()));
  CHECK_FALSE(caors::meta::find(outer.value_type()));

  caors::any_ref first_row = outer.element_ref(0);
  REQUIRE(first_row.valid());
  caors::value::sequence_view inner = first_row.create_sequence_view();
  REQUIRE(inner.valid());
  CHECK(inner.value_type() == caors::meta::id("int"));

  caors::any_ref second = inner.element_ref(1);
  REQUIRE(second.valid());
  CHECK(second.try_set(8));
  CHECK(values[0][1] == 8);

  CHECK(outer.append_value(caors::any::from(std::vector<int>{13, 21})));
  REQUIRE(values.size() == 3);
  CHECK(values[2][0] == 13);
}

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
  CHECK(values_field.type_name().empty());
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
  CHECK(inner.element_type() == caors::meta::id("int"));
  CHECK(inner.element_ref(0).try_set(5));
  CHECK(holder.values[0][0] == 5);
}
