// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "reflection_test_support.h"

#include "caors/value/associative_entry_ref.h"
#include "caors/value/view_factory.h"

using namespace test_reflection;

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
