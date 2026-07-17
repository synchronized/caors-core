// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：meta_tuple_variant_view_std_pair_field_view_exposes_first_and_second_values_test.cpp

#include "meta_field_test_support.h"

#include "caors/meta/value/associative_entry_ref.h"
#include "caors/meta/value/type_capability.h"

using namespace test_meta;

TEST_CASE("meta - std pair field view exposes first and second values", "[meta]") {
  REQUIRE(((void)caors::meta::class_<pair_holder>("caors.test.meta.pair_holder"),
           caors::meta::class_<pair_holder>())
              .field("value", &pair_holder::value)
              .commit()
              .succeeded());

  auto reflected = caors::meta::find("caors.test.meta.pair_holder");
  REQUIRE(reflected);

  auto value = reflected.find_field("value");
  REQUIRE(value);
  CHECK(value.kind() == caors::meta::type_kind::pair);
  CHECK(value.is_pair());
  CHECK(value.get_type().name() == "std.pair<int,caors.string>");
  CHECK(value.get_first_type().id() == registered_type_id("int"));
  CHECK(value.get_first_type().name() == "int");
  CHECK(value.get_second_type().id() == registered_type_id("caors.string"));
  CHECK(value.get_second_type().name() == "caors.string");
  CHECK(value.get_value_type().id() == registered_type_id("caors.string"));

  pair_holder instance{{7, caors::string{"seven"}}};
  caors::meta::pair_view pair =
      value.get_ref(&instance).get_capability<caors::meta::pair_capability>();
  REQUIRE(pair.valid());
  CHECK(pair.writable());
  CHECK(pair.get_first_type().id() == registered_type_id("int"));
  CHECK(pair.get_second_type().id() == registered_type_id("caors.string"));

  caors::any_ref first = pair.first_ref();
  REQUIRE(first.valid());
  CHECK(first.try_set(9));

  caors::any_ref second = pair.second_ref();
  REQUIRE(second.valid());
  CHECK(second.try_set(caors::string{"nine"}));

  CHECK(instance.value.first == 9);
  CHECK(instance.value.second == "nine");
}

// 原测试文件：meta_tuple_variant_view_std_tuple_field_view_exposes_indexed_elements_test.cpp

#include "meta_field_test_support.h"

#include "caors/meta/value/associative_entry_ref.h"
#include "caors/meta/value/type_capability.h"

using namespace test_meta;

TEST_CASE("meta - std tuple field view exposes indexed elements", "[meta]") {
  REQUIRE(((void)caors::meta::class_<tuple_holder>("caors.test.meta.tuple_holder"),
           caors::meta::class_<tuple_holder>())
              .field("value", &tuple_holder::value)
              .commit()
              .succeeded());

  auto reflected = caors::meta::find("caors.test.meta.tuple_holder");
  REQUIRE(reflected);

  auto value = reflected.find_field("value");
  REQUIRE(value);
  CHECK(value.kind() == caors::meta::type_kind::tuple);
  CHECK(value.is_tuple());
  CHECK(value.get_type().name() == "std.tuple<int,caors.string,float>");

  tuple_holder instance{std::tuple<int, caors::string, float>{7, caors::string{"seven"}, 1.5F}};
  caors::meta::tuple_view tuple =
      value.get_ref(&instance).get_capability<caors::meta::tuple_capability>();
  REQUIRE(tuple.valid());
  CHECK(tuple.writable());
  CHECK(tuple.size() == 3);

  REQUIRE(tuple.get_element_type(0).has_value());
  REQUIRE(tuple.get_element_type(1).has_value());
  REQUIRE(tuple.get_element_type(2).has_value());
  CHECK(tuple.get_element_type(0)->id() == registered_type_id("int"));
  CHECK(tuple.get_element_type(1)->id() == registered_type_id("caors.string"));
  CHECK(tuple.get_element_type(2)->id() == registered_type_id("float"));
  CHECK(tuple.get_element_type(1)->name() == "caors.string");

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

// 原测试文件：meta_tuple_variant_view_std_variant_field_view_exposes_active_alternative_test.cpp

#include "meta_field_test_support.h"

#include "caors/meta/value/associative_entry_ref.h"
#include "caors/meta/value/type_capability.h"

using namespace test_meta;

TEST_CASE("meta - std variant field view exposes active alternative", "[meta]") {
  REQUIRE(((void)caors::meta::class_<variant_holder>("caors.test.meta.variant_holder"),
           caors::meta::class_<variant_holder>())
              .field("value", &variant_holder::value)
              .commit()
              .succeeded());

  auto reflected = caors::meta::find("caors.test.meta.variant_holder");
  REQUIRE(reflected);

  auto value = reflected.find_field("value");
  REQUIRE(value);
  CHECK(value.kind() == caors::meta::type_kind::variant);
  CHECK(value.is_variant());
  CHECK(value.get_type().name() == "std.variant<int,caors.string,float>");

  variant_holder instance{std::variant<int, caors::string, float>{7}};
  caors::meta::variant_view variant =
      value.get_ref(&instance).get_capability<caors::meta::variant_capability>();
  REQUIRE(variant.valid());
  CHECK(variant.writable());
  CHECK(variant.size() == 3);

  REQUIRE(variant.index().has_value());
  CHECK(*variant.index() == 0);
  REQUIRE(variant.get_alternative_type(0).has_value());
  REQUIRE(variant.get_alternative_type(1).has_value());
  REQUIRE(variant.get_alternative_type(2).has_value());
  CHECK(variant.get_alternative_type(0)->id() == registered_type_id("int"));
  CHECK(variant.get_alternative_type(1)->id() == registered_type_id("caors.string"));
  CHECK(variant.get_alternative_type(2)->id() == registered_type_id("float"));
  CHECK(variant.get_alternative_type(1)->name() == "caors.string");
  CHECK(variant.get_value_type().id() == registered_type_id("int"));

  caors::any_ref active = variant.value_ref();
  REQUIRE(active.valid());
  CHECK(active.try_set(9));
  CHECK(std::get<int>(instance.value) == 9);

  REQUIRE(variant.emplace(1));
  REQUIRE(variant.index().has_value());
  CHECK(*variant.index() == 1);
  CHECK(variant.get_value_type().name() == "caors.string");

  active = variant.value_ref();
  REQUIRE(active.valid());
  CHECK(active.try_set(caors::string{"nine"}));
  CHECK(std::get<caors::string>(instance.value) == "nine");
}
