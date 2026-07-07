// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "reflection_test_support.h"

#include "caors/value/associative_entry_ref.h"
#include "caors/value/view_factory.h"

using namespace test_reflection;

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
