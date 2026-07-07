// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

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
