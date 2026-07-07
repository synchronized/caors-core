// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "reflection_test_support.h"

#include "caors/value/associative_entry_ref.h"
#include "caors/value/view_factory.h"

using namespace test_reflection;

TEST_CASE("reflection - std tuple field view exposes indexed elements", "[reflection]") {
  REQUIRE(caors::reflection::class_<tuple_holder>("caors.test.reflection.tuple_holder")
              .field("value", &tuple_holder::value)
              .commit()
              .succeeded());

  auto reflected = caors::reflection::find("caors.test.reflection.tuple_holder");
  REQUIRE(reflected);

  auto value = reflected.find_field("value");
  REQUIRE(value);
  CHECK(value.kind() == caors::meta::type_kind::tuple);
  CHECK(value.is_tuple());
  CHECK(value.type_name() == "std.tuple<int,caors.string,float>");

  tuple_holder instance{std::tuple<int, caors::string, float>{7, caors::string{"seven"}, 1.5F}};
  caors::value::tuple_view tuple = value.get_ref(&instance).create_tuple_view();
  REQUIRE(tuple.valid());
  CHECK(tuple.writable());
  CHECK(tuple.size() == 3);

  REQUIRE(tuple.element_type(0).has_value());
  REQUIRE(tuple.element_type(1).has_value());
  REQUIRE(tuple.element_type(2).has_value());
  CHECK(*tuple.element_type(0) == registered_type_id("int"));
  CHECK(*tuple.element_type(1) == registered_type_id("caors.string"));
  CHECK(*tuple.element_type(2) == registered_type_id("float"));
  CHECK(tuple.element_type_name(1) == "caors.string");

  caors::any_ref first = tuple.element_ref(0);
  caors::any_ref second = tuple.element_ref(1);
  caors::any_ref third = tuple.element_ref(2);
  REQUIRE(first.valid());
  REQUIRE(second.valid());
  REQUIRE(third.valid());

  CHECK(first.try_set(9));
  CHECK(second.try_set(caors::string{"nine"}));
  CHECK(third.try_set(2.5F));

  CHECK(std::get<0>(instance.value) == 9);
  CHECK(std::get<1>(instance.value) == "nine");
  CHECK(std::get<2>(instance.value) == 2.5F);
}
