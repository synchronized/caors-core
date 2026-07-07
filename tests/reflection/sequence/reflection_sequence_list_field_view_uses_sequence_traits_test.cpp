// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "reflection_test_support.h"

#include "caors/value/associative_entry_ref.h"
#include "caors/value/view_factory.h"

using namespace test_reflection;

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
