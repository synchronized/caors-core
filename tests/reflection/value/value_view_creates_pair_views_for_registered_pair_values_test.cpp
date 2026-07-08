// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "value_test_common.h"

using namespace caors::tests::reflection;

TEST_CASE("reflection any_ref - creates pair views from descriptor values", "[reflection]") {
  require_value_test_meta();

  std::pair<int, int> value{3, 5};

  caors::any_ref reference = caors::any_ref::from(value);
  REQUIRE(reference.valid());

  caors::value::pair_view pair = reference.create_pair_view();
  REQUIRE(pair.valid());
  CHECK(pair.writable());
  const caors::type_id int_type = caors::any::from(0).type();
  CHECK(pair.first_type() == int_type);
  CHECK(pair.second_type() == int_type);

  caors::any_ref first = pair.first_ref();
  caors::any_ref second = pair.second_ref();
  REQUIRE(first.valid());
  REQUIRE(second.valid());

  CHECK(first.try_set(7));
  CHECK(second.try_set(11));
  CHECK(value.first == 7);
  CHECK(value.second == 11);
}
