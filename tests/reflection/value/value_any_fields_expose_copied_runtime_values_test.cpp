// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "value_test_common.h"

using namespace caors::tests::reflection;

TEST_CASE("reflection any - fields expose copied runtime values", "[reflection]") {
  require_value_test_meta();

  REQUIRE(caors::reflection::class_<reflected_value_holder>(
              "caors.test.reflection.value_reflected_holder")
              .field("value", &reflected_value_holder::value)
              .field("readonly", &reflected_value_holder::readonly)
              .commit() == caors::error{});

  reflected_value_holder holder{1, 7};
  const caors::reflection::field value =
      require_reflected_field("caors.test.reflection.value_reflected_holder", "value");
  const caors::reflection::field readonly =
      require_reflected_field("caors.test.reflection.value_reflected_holder", "readonly");

  caors::any copied = value.get_value(&holder);
  CHECK(copied.valid());
  CHECK(copied.matches_type<int>());
  REQUIRE(copied.try_get<int>() != nullptr);
  CHECK(*copied.try_get<int>() == 1);

  CHECK(copied.try_set(10));
  CHECK(*copied.try_get<int>() == 10);
  CHECK(holder.value == 1);

  CHECK(value.try_set_value(&holder, copied));
  CHECK(holder.value == 10);

  caors::any mismatch = caors::any::from(2.0F);
  CHECK_FALSE(value.try_set_value(&holder, mismatch));
  CHECK(holder.value == 10);

  caors::any readonly_value = readonly.get_value(&holder);
  CHECK(readonly_value.valid());
  REQUIRE(readonly_value.try_get<int>() != nullptr);
  CHECK(*readonly_value.try_get<int>() == 7);
  CHECK(readonly_value.try_set(8));
  CHECK(*readonly_value.try_get<int>() == 8);
  CHECK_FALSE(readonly.try_set_value(&holder, readonly_value));
  CHECK(holder.readonly == 7);
}
