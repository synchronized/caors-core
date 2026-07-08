// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 鍘熸祴璇曟枃浠讹細reflection_tuple_variant_view_std_pair_field_view_exposes_first_and_second_values_test.cpp

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

// 鍘熸祴璇曟枃浠讹細reflection_tuple_variant_view_std_tuple_field_view_exposes_indexed_elements_test.cpp

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

// 鍘熸祴璇曟枃浠讹細reflection_tuple_variant_view_std_variant_field_view_exposes_active_alternative_test.cpp

#include "reflection_test_support.h"

#include "caors/value/associative_entry_ref.h"
#include "caors/value/view_factory.h"

using namespace test_reflection;

TEST_CASE("reflection - std variant field view exposes active alternative", "[reflection]") {
  REQUIRE(caors::reflection::class_<variant_holder>("caors.test.reflection.variant_holder")
              .field("value", &variant_holder::value)
              .commit()
              .succeeded());

  auto reflected = caors::reflection::find("caors.test.reflection.variant_holder");
  REQUIRE(reflected);

  auto value = reflected.find_field("value");
  REQUIRE(value);
  CHECK(value.kind() == caors::meta::type_kind::variant);
  CHECK(value.is_variant());
  CHECK(value.type_name() == "std.variant<int,caors.string,float>");

  variant_holder instance{std::variant<int, caors::string, float>{7}};
  caors::value::variant_view variant = value.get_ref(&instance).create_variant_view();
  REQUIRE(variant.valid());
  CHECK(variant.writable());
  CHECK(variant.size() == 3);

  REQUIRE(variant.index().has_value());
  CHECK(*variant.index() == 0);
  REQUIRE(variant.alternative_type(0).has_value());
  REQUIRE(variant.alternative_type(1).has_value());
  REQUIRE(variant.alternative_type(2).has_value());
  CHECK(*variant.alternative_type(0) == registered_type_id("int"));
  CHECK(*variant.alternative_type(1) == registered_type_id("caors.string"));
  CHECK(*variant.alternative_type(2) == registered_type_id("float"));
  CHECK(variant.alternative_type_name(1) == "caors.string");
  CHECK(variant.value_type() == registered_type_id("int"));

  caors::any_ref active = variant.value_ref();
  REQUIRE(active.valid());
  CHECK(active.try_set(9));
  CHECK(std::get<int>(instance.value) == 9);

  REQUIRE(variant.emplace(1));
  REQUIRE(variant.index().has_value());
  CHECK(*variant.index() == 1);
  CHECK(variant.value_type_name() == "caors.string");

  active = variant.value_ref();
  REQUIRE(active.valid());
  CHECK(active.try_set(caors::string{"nine"}));
  CHECK(std::get<caors::string>(instance.value) == "nine");
}
