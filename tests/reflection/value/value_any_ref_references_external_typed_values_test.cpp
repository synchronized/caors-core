// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "value_test_common.h"

using namespace caors::tests::reflection;

TEST_CASE("reflection any_ref - references external typed values", "[reflection]") {
  require_value_test_meta();

  int source = 7;
  caors::any_ref reference = caors::any_ref::from(source);

  CHECK(reference.valid());
  CHECK(reference.writable());
  CHECK(reference.matches_type<int>());
  REQUIRE(reference.try_get<int>() != nullptr);
  CHECK(*reference.try_get<int>() == 7);

  int* writable = reference.try_get_mutable<int>();
  REQUIRE(writable != nullptr);
  *writable = 11;
  CHECK(source == 11);
  CHECK(reference.try_set(13));
  CHECK(source == 13);

  caors::any copied = caors::any::from(17);
  CHECK(reference.try_set(copied));
  CHECK(source == 17);

  const int readonly = 3;
  caors::any_ref readonly_reference = caors::any_ref::from_const(readonly);
  CHECK(readonly_reference.valid());
  CHECK_FALSE(readonly_reference.writable());
  CHECK(readonly_reference.try_get_mutable<int>() == nullptr);
  CHECK_FALSE(readonly_reference.try_set(4));
  REQUIRE(readonly_reference.try_get<int>() != nullptr);
  CHECK(*readonly_reference.try_get<int>() == 3);

  int target = 0;
  caors::any_ref target_reference = caors::any_ref::from(target);
  CHECK(target_reference.try_set(readonly_reference));
  CHECK(target == 3);
}
