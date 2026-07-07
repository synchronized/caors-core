// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "value_test_common.h"

using namespace caors::tests::reflection;

TEST_CASE("reflection any_ref - creates descriptor views for unregistered nested vectors",
          "[reflection]") {
  require_value_test_meta();

  std::vector<std::vector<int>> values{{1, 2}, {3}};

  caors::any_ref reference = caors::any_ref::from(values);
  REQUIRE(reference.valid());
  CHECK(reference.type_name().empty());

  caors::value::sequence_view outer = reference.create_sequence_view();
  REQUIRE(outer.valid());
  CHECK(outer.writable());
  CHECK(static_cast<bool>(outer.value_type()));
  CHECK_FALSE(caors::meta::find(outer.value_type()));

  caors::any_ref first_row = outer.element_ref(0);
  REQUIRE(first_row.valid());
  caors::value::sequence_view inner = first_row.create_sequence_view();
  REQUIRE(inner.valid());
  CHECK(inner.value_type() == caors::meta::id("int"));

  caors::any_ref second = inner.element_ref(1);
  REQUIRE(second.valid());
  CHECK(second.try_set(8));
  CHECK(values[0][1] == 8);

  CHECK(outer.append_value(caors::any::from(std::vector<int>{13, 21})));
  REQUIRE(values.size() == 3);
  CHECK(values[2][0] == 13);
}
