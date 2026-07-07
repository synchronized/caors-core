// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "value_test_common.h"

using namespace caors::tests::reflection;

TEST_CASE("reflection any_ref - creates pair views for registered pair values", "[reflection]") {
  require_value_test_meta();

  std::pair<int, int> value{3, 5};

  caors::any_ref reference = caors::any_ref::from(value, "std.pair<int,int>");
  REQUIRE(reference.valid());

  caors::value::pair_view pair = reference.create_pair_view();
  REQUIRE(pair.valid());
  CHECK(pair.writable());
  CHECK(pair.first_type() == caors::meta::id("int"));
  CHECK(pair.second_type() == caors::meta::id("int"));

  caors::any_ref first = pair.first_ref();
  caors::any_ref second = pair.second_ref();
  REQUIRE(first.valid());
  REQUIRE(second.valid());

  CHECK(first.try_set(7));
  CHECK(second.try_set(11));
  CHECK(value.first == 7);
  CHECK(value.second == 11);
}
