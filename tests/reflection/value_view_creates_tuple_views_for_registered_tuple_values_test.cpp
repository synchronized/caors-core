// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "value_test_common.h"

using namespace caors::tests::reflection;

TEST_CASE("reflection any_ref - creates tuple views for registered tuple values", "[reflection]") {
  require_value_test_meta();

  std::tuple<int, int, caors::string> value{3, 5, caors::string{"five"}};

  caors::any_ref reference = caors::any_ref::from(value, "std.tuple<int,int,caors.string>");
  REQUIRE(reference.valid());

  caors::value::tuple_view tuple = reference.create_tuple_view();
  REQUIRE(tuple.valid());
  CHECK(tuple.writable());
  CHECK(tuple.size() == 3);
  REQUIRE(tuple.element_type(0).has_value());
  CHECK(*tuple.element_type(0) == caors::meta::id("int"));
  CHECK(tuple.element_type_name(2) == "caors.string");

  caors::any_ref first = tuple.element_ref(0);
  caors::any_ref second = tuple.element_ref(1);
  caors::any_ref third = tuple.element_ref(2);
  REQUIRE(first.valid());
  REQUIRE(second.valid());
  REQUIRE(third.valid());

  CHECK(first.try_set(7));
  CHECK(second.try_set(11));
  CHECK(third.try_set(caors::string{"eleven"}));
  CHECK(std::get<0>(value) == 7);
  CHECK(std::get<1>(value) == 11);
  CHECK(std::get<2>(value) == "eleven");
}
