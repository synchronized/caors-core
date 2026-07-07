// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "reflection_test_support.h"

#include "caors/value/associative_entry_ref.h"
#include "caors/value/view_factory.h"

using namespace test_reflection;

TEST_CASE("reflection - std pair field view exposes first and second values", "[reflection]") {
  REQUIRE(caors::reflection::class_<pair_holder>("caors.test.reflection.pair_holder")
              .field("value", &pair_holder::value)
              .commit()
              .succeeded());

  auto reflected = caors::reflection::find("caors.test.reflection.pair_holder");
  REQUIRE(reflected);

  auto value = reflected.find_field("value");
  REQUIRE(value);
  CHECK(value.kind() == caors::meta::type_kind::pair);
  CHECK(value.is_pair());
  CHECK(value.type_name() == "std.pair<int,caors.string>");
  CHECK(value.first_type() == registered_type_id("int"));
  CHECK(value.first_type_name() == "int");
  CHECK(value.second_type() == registered_type_id("caors.string"));
  CHECK(value.second_type_name() == "caors.string");
  CHECK(value.value_type() == registered_type_id("caors.string"));

  pair_holder instance{{7, caors::string{"seven"}}};
  caors::value::pair_view pair = value.get_ref(&instance).create_pair_view();
  REQUIRE(pair.valid());
  CHECK(pair.writable());
  CHECK(pair.first_type() == registered_type_id("int"));
  CHECK(pair.second_type() == registered_type_id("caors.string"));

  caors::any_ref first = pair.first_ref();
  REQUIRE(first.valid());
  CHECK(first.try_set(9));

  caors::any_ref second = pair.second_ref();
  REQUIRE(second.valid());
  CHECK(second.try_set(caors::string{"nine"}));

  CHECK(instance.value.first == 9);
  CHECK(instance.value.second == "nine");
}
