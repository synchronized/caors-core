// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "reflection_test_support.h"

#include "caors/value/associative_entry_ref.h"
#include "caors/value/view_factory.h"

using namespace test_reflection;

TEST_CASE("reflection - sequence field view exposes element metadata", "[reflection]") {
  REQUIRE(caors::reflection::class_<vector_holder>("caors.test.reflection.vector_holder")
              .field("values", &vector_holder::values)
              .commit()
              .succeeded());

  auto reflected = caors::reflection::find("caors.test.reflection.vector_holder");
  REQUIRE(reflected);

  auto values = reflected.find_field("values");
  REQUIRE(values);
  CHECK(values.kind() == caors::meta::type_kind::sequence);
  CHECK(values.is_sequence());
  CHECK(values.value_type() == registered_type_id("int"));
  CHECK(values.element_type() == registered_type_id("int"));
  CHECK(values.element_type_name() == "int");

  vector_holder instance{};
  instance.values.push_back(3);
  instance.values.push_back(5);

  caors::value::sequence_view sequence = values.get_ref(&instance).create_sequence_view();
  CHECK(sequence.valid());
  CHECK(sequence.writable());
  CHECK(sequence.value_type() == registered_type_id("int"));
  CHECK(sequence.value_type_name() == "int");
  CHECK(sequence.element_type() == registered_type_id("int"));
  CHECK(sequence.element_type_name() == "int");

  int sequence_sum = 0;
  for (caors::any_ref value : sequence) {
    const int* typed = value.try_get<int>();
    REQUIRE(typed != nullptr);
    sequence_sum += *typed;
  }
  CHECK(sequence_sum == 8);

  const std::optional<std::size_t> size = sequence.size();
  REQUIRE(size.has_value());
  CHECK(*size == 2);

  const vector_holder& readonly_instance = instance;
  caors::value::sequence_view readonly_sequence =
      values.get_ref(&readonly_instance).create_sequence_view();
  CHECK(readonly_sequence.valid());
  CHECK_FALSE(readonly_sequence.writable());

  caors::any_ref first = readonly_sequence.element_ref(0);
  REQUIRE(first.valid());
  CHECK_FALSE(first.writable());
  const int* first_value = first.try_get<int>();
  REQUIRE(first_value != nullptr);
  CHECK(*first_value == 3);

  caors::any_ref second = sequence.element_ref(1);
  REQUIRE(second.valid());
  CHECK(second.writable());
  CHECK(second.try_set<int>(8));
  CHECK(instance.values[1] == 8);

  CHECK(sequence.append_value(caors::any::from(13)));
  REQUIRE(instance.values.size() == 3);
  CHECK(instance.values[2] == 13);

  CHECK(sequence.resize(4));
  REQUIRE(instance.values.size() == 4);
  CHECK(instance.values[3] == 0);

  CHECK(sequence.clear());
  CHECK(instance.values.empty());
}

TEST_CASE("reflection - custom sequence traits expose field view", "[reflection]") {
  REQUIRE(caors::reflection::class_<custom_sequence_holder>(
              "caors.test.reflection.custom_sequence_holder")
              .field("values", &custom_sequence_holder::values)
              .commit()
              .succeeded());

  auto reflected = caors::reflection::find("caors.test.reflection.custom_sequence_holder");
  REQUIRE(reflected);

  auto values = reflected.find_field("values");
  REQUIRE(values);
  CHECK(values.kind() == caors::meta::type_kind::sequence);
  CHECK(values.type_name() == "test.custom_sequence<int>");
  CHECK(values.element_type() == registered_type_id("int"));
  CHECK(values.element_type_name() == "int");

  custom_sequence_holder instance{};
  caors::value::sequence_view sequence = values.get_ref(&instance).create_sequence_view();
  CHECK(sequence.append_value(caors::any::from(21)));
  CHECK(sequence.append_value(caors::any::from(34)));

  const std::optional<std::size_t> size = sequence.size();
  REQUIRE(size.has_value());
  CHECK(*size == 2);

  caors::any_ref first = sequence.element_ref(0);
  REQUIRE(first.valid());
  const int* first_value = first.try_get<int>();
  REQUIRE(first_value != nullptr);
  CHECK(*first_value == 21);

  CHECK(sequence.resize(1));
  CHECK(instance.values.values.size() == 1);
  CHECK(sequence.clear());
  CHECK(instance.values.values.empty());
}

TEST_CASE("reflection - std array field view uses fixed sequence traits", "[reflection]") {
  REQUIRE(caors::reflection::class_<array_holder>("caors.test.reflection.array_holder")
              .field("values", &array_holder::values)
              .commit()
              .succeeded());

  auto reflected = caors::reflection::find("caors.test.reflection.array_holder");
  REQUIRE(reflected);

  auto values = reflected.find_field("values");
  REQUIRE(values);
  CHECK(values.kind() == caors::meta::type_kind::sequence);
  CHECK(values.type_name() == "std.array<int,3>");
  CHECK(values.element_type() == registered_type_id("int"));

  array_holder instance{{1, 2, 3}};
  caors::value::sequence_view sequence = values.get_ref(&instance).create_sequence_view();
  REQUIRE(sequence.valid());
  CHECK(sequence.writable());

  const std::optional<std::size_t> size = sequence.size();
  REQUIRE(size.has_value());
  CHECK(*size == 3);

  caors::any_ref second = sequence.element_ref(1);
  REQUIRE(second.valid());
  CHECK(second.try_set(8));
  CHECK(instance.values[1] == 8);

  CHECK(sequence.resize(3));
  CHECK_FALSE(sequence.resize(2));
  CHECK_FALSE(sequence.append_value(caors::any::from(13)));
}

TEST_CASE("reflection - std span field view is borrowed sequence", "[reflection]") {
  REQUIRE(caors::reflection::class_<span_holder>("caors.test.reflection.span_holder")
              .field("values", &span_holder::values)
              .commit()
              .succeeded());

  auto reflected = caors::reflection::find("caors.test.reflection.span_holder");
  REQUIRE(reflected);

  auto values = reflected.find_field("values");
  REQUIRE(values);
  CHECK(values.kind() == caors::meta::type_kind::sequence);
  CHECK(values.type_name() == "std.span<int>");
  CHECK(values.element_type() == registered_type_id("int"));

  std::array<int, 3> storage{1, 2, 3};
  span_holder instance{std::span<int>{storage}};
  caors::value::sequence_view sequence = values.get_ref(&instance).create_sequence_view();
  REQUIRE(sequence.valid());
  CHECK(sequence.writable());

  const std::optional<std::size_t> size = sequence.size();
  REQUIRE(size.has_value());
  CHECK(*size == 3);

  caors::any_ref second = sequence.element_ref(1);
  REQUIRE(second.valid());
  CHECK(second.try_set(8));
  CHECK(storage[1] == 8);

  CHECK(sequence.resize(3));
  CHECK_FALSE(sequence.resize(2));
  CHECK_FALSE(sequence.append_value(caors::any::from(13)));
}

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

TEST_CASE("reflection - list field view uses sequence traits", "[reflection]") {
  REQUIRE(caors::reflection::class_<list_holder>("caors.test.reflection.list_holder")
              .field("values", &list_holder::values)
              .commit()
              .succeeded());

  auto reflected = caors::reflection::find("caors.test.reflection.list_holder");
  REQUIRE(reflected);

  auto values = reflected.find_field("values");
  REQUIRE(values);
  CHECK(values.kind() == caors::meta::type_kind::sequence);
  CHECK(values.type_name() == "caors.list<int>");
  CHECK(values.element_type() == registered_type_id("int"));

  list_holder instance{};
  instance.values.push_back(3);
  instance.values.push_back(5);

  caors::value::sequence_view sequence = values.get_ref(&instance).create_sequence_view();
  CHECK(sequence.valid());
  CHECK(sequence.writable());

  caors::any_ref second = sequence.element_ref(1);
  REQUIRE(second.valid());
  CHECK(second.try_set(8));

  auto iterator = instance.values.begin();
  std::advance(iterator, 1);
  CHECK(*iterator == 8);

  CHECK(sequence.append_value(caors::any::from(13)));
  CHECK(instance.values.back() == 13);
}
