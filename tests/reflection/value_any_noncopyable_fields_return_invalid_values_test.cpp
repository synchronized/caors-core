// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "value_test_common.h"

using namespace caors::tests::reflection;

TEST_CASE("reflection any - noncopyable fields return invalid values", "[reflection]") {
  require_value_test_meta();

  REQUIRE(caors::reflection::class_<move_only_value>("caors.test.reflection.move_only_value")
              .field("value", &move_only_value::value)
              .commit() == caors::error{});

  move_only_value holder{std::make_unique<int>(7)};
  const caors::reflection::field field =
      require_reflected_field("caors.test.reflection.move_only_value", "value");

  caors::any copied = field.get_value(&holder);
  CHECK_FALSE(copied.valid());

  const move_only_value& readonly_holder = holder;
  caors::any_ref referenced = field.get_ref(&readonly_holder);
  CHECK(referenced.valid());
  CHECK_FALSE(referenced.writable());
  const std::unique_ptr<int>* stored = referenced.try_get<std::unique_ptr<int>>();
  REQUIRE(stored != nullptr);
  REQUIRE(*stored);
  CHECK(**stored == 7);

  caors::any_ref writable = field.get_ref(&holder);
  CHECK(writable.valid());
  CHECK(writable.writable());
  std::unique_ptr<int>* writable_stored = writable.try_get_mutable<std::unique_ptr<int>>();
  REQUIRE(writable_stored != nullptr);
  *writable_stored = std::make_unique<int>(9);
  REQUIRE(holder.value);
  CHECK(*holder.value == 9);

  std::unique_ptr<int> replacement = std::make_unique<int>(13);
  CHECK_FALSE(writable.try_set(replacement));

  int numeric = 15;
  caors::any_ref numeric_ref = caors::any_ref::from(numeric);
  CHECK(numeric_ref.try_set(21));
  CHECK(numeric == 21);
}
