// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：value_indirect_view_creates_indirect_views_for_pointer_and_wrappers_test.cpp

#include "value_test_common.h"

using namespace caors::tests::reflection;

TEST_CASE("reflection any_ref - creates indirect views for pointer and wrappers", "[reflection]") {
  require_value_test_meta();

  REQUIRE(caors::reflection::class_<indirect_value_holder>(
              "caors.test.reflection.indirect_value_holder")
              .field("pointer", &indirect_value_holder::pointer)
              .field("optional", &indirect_value_holder::optional)
              .field("unique", &indirect_value_holder::unique)
              .field("weak", &indirect_value_holder::weak)
              .commit() == caors::error{});

  indirect_value_holder holder{};

  const caors::reflection::field pointer_field =
      require_reflected_field("caors.test.reflection.indirect_value_holder", "pointer");
  const caors::reflection::field optional_field =
      require_reflected_field("caors.test.reflection.indirect_value_holder", "optional");
  const caors::reflection::field unique_field =
      require_reflected_field("caors.test.reflection.indirect_value_holder", "unique");
  const caors::reflection::field weak_field =
      require_reflected_field("caors.test.reflection.indirect_value_holder", "weak");

  caors::value::indirect_view pointer = pointer_field.get_ref(&holder).create_indirect_view();
  REQUIRE(pointer.valid());
  CHECK(pointer.writable());
  CHECK(pointer.get_value_type().id() == caors::any::from(0).get_type().id());
  CHECK(pointer.get_value_type().id() == caors::any::from(0).get_type().id());
  CHECK(pointer.get_dynamic_value_type().id() == caors::any::from(0).get_type().id());
  CHECK(pointer.get_value_type().name() == "int");
  CHECK(pointer.has_value());
  caors::any_ref pointer_value = pointer.value_ref();
  REQUIRE(pointer_value.valid());
  REQUIRE(pointer_value.try_get_mutable<int>() != nullptr);
  *pointer_value.try_get_mutable<int>() = 23;
  CHECK(holder.value == 23);
  CHECK(pointer.reset());
  CHECK(holder.pointer == nullptr);
  CHECK_FALSE(pointer.has_value());
  CHECK_FALSE(pointer.value_ref().valid());

  caors::value::indirect_view optional = optional_field.get_ref(&holder).create_indirect_view();
  REQUIRE(optional.valid());
  REQUIRE(optional.value_ref().try_get_mutable<int>() != nullptr);
  CHECK(*optional.value_ref().try_get_mutable<int>() == 11);
  CHECK(optional.value_ref().try_set(31));
  CHECK(holder.optional == 31);
  CHECK(optional.reset());
  CHECK_FALSE(holder.optional.has_value());
  CHECK_FALSE(optional.has_value());

  caors::value::indirect_view unique = unique_field.get_ref(&holder).create_indirect_view();
  REQUIRE(unique.valid());
  REQUIRE(unique.value_ref().try_get_mutable<int>() != nullptr);
  CHECK(*unique.value_ref().try_get_mutable<int>() == 13);
  CHECK(unique.value_ref().try_set(37));
  REQUIRE(holder.unique);
  CHECK(*holder.unique == 37);

  const indirect_value_holder& readonly_holder = holder;
  caors::value::indirect_view readonly_unique =
      unique_field.get_ref(&readonly_holder).create_indirect_view();
  REQUIRE(readonly_unique.valid());
  CHECK_FALSE(readonly_unique.writable());
  caors::any_ref readonly_value = readonly_unique.value_ref();
  CHECK(readonly_value.valid());
  CHECK_FALSE(readonly_value.writable());
  CHECK_FALSE(readonly_unique.reset());

  CHECK_FALSE(weak_field.get_ref(&holder).create_indirect_view().valid());
}

// 原测试文件：value_indirect_view_creates_indirect_views_for_registered_wrapper_values_test.cpp

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
  CHECK(indirect.get_value_type().id() == caors::any::from(0).get_type().id());

  caors::any_ref inner = indirect.value_ref();
  REQUIRE(inner.valid());
  CHECK(inner.try_set(43));
  CHECK(value == 43);

  CHECK(indirect.reset());
  CHECK_FALSE(value.has_value());
  CHECK_FALSE(indirect.has_value());
}
