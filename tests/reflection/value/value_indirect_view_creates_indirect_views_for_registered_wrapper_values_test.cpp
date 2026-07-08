// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "value_test_common.h"

using namespace caors::tests::reflection;

TEST_CASE("reflection any_ref - creates indirect views from descriptor wrapper values",
          "[reflection]") {
  require_value_test_meta();

  std::optional<int> value = 41;

  caors::any_ref reference = caors::any_ref::from(value);
  REQUIRE(reference.valid());

  caors::value::indirect_view indirect = reference.create_indirect_view();
  REQUIRE(indirect.valid());
  CHECK(indirect.has_value());
  CHECK(indirect.value_type() == caors::any::from(0).type());

  caors::any_ref inner = indirect.value_ref();
  REQUIRE(inner.valid());
  CHECK(inner.try_set(43));
  CHECK(value == 43);

  CHECK(indirect.reset());
  CHECK_FALSE(value.has_value());
  CHECK_FALSE(indirect.has_value());
}
