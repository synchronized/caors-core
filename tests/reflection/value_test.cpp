// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/reflection.h"

#include <catch2/catch_all.hpp>

#include <cstddef>
#include <memory>
#include <optional>
#include <utility>

namespace {

struct reflected_value_holder {
  int value = 0;
  const int readonly = 7;
};

struct move_only_value {
  std::unique_ptr<int> value{};
};

struct indirect_value_holder {
  int value = 7;
  int* pointer = &value;
  std::optional<int> optional = 11;
  std::unique_ptr<int> unique = std::make_unique<int>(13);
  std::weak_ptr<int> weak{};
};

} // namespace

TEST_CASE("reflection any - stores copied values", "[reflection]") {
  int source = 7;
  caors::any stored = caors::any::from(source);

  CHECK(stored.valid());
  const auto int_type = caors::find_type("int");
  REQUIRE(int_type.has_value());
  CHECK(stored.type() == int_type->id());
  CHECK(stored.type_name() == "int");
  CHECK(stored.matches_type<int>());
  CHECK(stored.size() == sizeof(int));
  CHECK(stored.alignment() == alignof(int));

  source = 8;
  const int* read = stored.try_get<int>();
  REQUIRE(read != nullptr);
  CHECK(*read == 7);

  int* write = stored.try_get_mutable<int>();
  REQUIRE(write != nullptr);
  *write = 9;
  CHECK(source == 8);
  CHECK(*stored.try_get<int>() == 9);

  CHECK(stored.try_set(10));
  CHECK(*stored.try_get<int>() == 10);
}

TEST_CASE("reflection any - copies deeply and rejects mismatched writes", "[reflection]") {
  caors::any stored = caors::any::from(7);
  caors::any copied = stored;

  REQUIRE(stored.try_set(8));
  REQUIRE(copied.try_get<int>() != nullptr);
  CHECK(*copied.try_get<int>() == 7);
  CHECK(*stored.try_get<int>() == 8);

  float other = 1.0F;
  CHECK_FALSE(stored.matches_type<float>());
  CHECK_FALSE(stored.try_set(caors::any::from(other)));
  CHECK(*stored.try_get<int>() == 8);

  caors::any moved = std::move(stored);
  CHECK(moved.valid());
  CHECK_FALSE(stored.valid());
  CHECK(*moved.try_get<int>() == 8);
}

TEST_CASE("reflection any_ref - references external typed values", "[reflection]") {
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

TEST_CASE("reflection any - fields expose copied runtime values", "[reflection]") {
  reflected_value_holder holder{1, 7};
  const caors::reflection_field_data value = caors::make_reflection_field_data<int>(
      "value", "int", offsetof(reflected_value_holder, value));
  const caors::reflection_field_data readonly = caors::make_reflection_field_data<const int>(
      "readonly", "int", offsetof(reflected_value_holder, readonly));

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

TEST_CASE("reflection any - noncopyable fields return invalid values", "[reflection]") {
  move_only_value holder{std::make_unique<int>(7)};
  const caors::reflection_field_data field =
      caors::make_reflection_field_data<std::unique_ptr<int>>("value", "std.unique_ptr<int>",
                                                              offsetof(move_only_value, value));

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
  const caors::reflection_field_data numeric_field =
      caors::make_reflection_field_data<int>("numeric", "int", 0);
  caors::any_ref numeric_ref = numeric_field.get_ref(&numeric);
  CHECK(numeric_ref.try_set(21));
  CHECK(numeric == 21);
}

TEST_CASE("reflection any_ref - creates indirect views for pointer and wrappers", "[reflection]") {
  indirect_value_holder holder{};
  auto shared = std::make_shared<int>(17);
  holder.weak = shared;

  const caors::reflection_field_data pointer_field = caors::make_reflection_field_data<int*>(
      "pointer", "int*", offsetof(indirect_value_holder, pointer));
  const caors::reflection_field_data optional_field =
      caors::make_reflection_field_data<std::optional<int>>(
          "optional", "std.optional<int>", offsetof(indirect_value_holder, optional));
  const caors::reflection_field_data unique_field =
      caors::make_reflection_field_data<std::unique_ptr<int>>(
          "unique", "std.unique_ptr<int>", offsetof(indirect_value_holder, unique));
  const caors::reflection_field_data weak_field =
      caors::make_reflection_field_data<std::weak_ptr<int>>("weak", "std.weak_ptr<int>",
                                                            offsetof(indirect_value_holder, weak));

  caors::indirect_view pointer = pointer_field.get_ref(&holder).create_indirect_view();
  REQUIRE(pointer.valid());
  CHECK(pointer.writable());
  CHECK(pointer.value_type() == caors::make_type_id("int"));
  CHECK(pointer.value_type_name() == "int");
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

  caors::indirect_view optional = optional_field.get_ref(&holder).create_indirect_view();
  REQUIRE(optional.valid());
  REQUIRE(optional.value_ref().try_get_mutable<int>() != nullptr);
  CHECK(*optional.value_ref().try_get_mutable<int>() == 11);
  CHECK(optional.value_ref().try_set(31));
  CHECK(holder.optional == 31);
  CHECK(optional.reset());
  CHECK_FALSE(holder.optional.has_value());
  CHECK_FALSE(optional.has_value());

  caors::indirect_view unique = unique_field.get_ref(&holder).create_indirect_view();
  REQUIRE(unique.valid());
  REQUIRE(unique.value_ref().try_get_mutable<int>() != nullptr);
  CHECK(*unique.value_ref().try_get_mutable<int>() == 13);
  CHECK(unique.value_ref().try_set(37));
  REQUIRE(holder.unique);
  CHECK(*holder.unique == 37);

  const indirect_value_holder& readonly_holder = holder;
  caors::indirect_view readonly_unique =
      unique_field.get_ref(&readonly_holder).create_indirect_view();
  REQUIRE(readonly_unique.valid());
  CHECK_FALSE(readonly_unique.writable());
  caors::any_ref readonly_value = readonly_unique.value_ref();
  CHECK(readonly_value.valid());
  CHECK_FALSE(readonly_value.writable());
  CHECK_FALSE(readonly_unique.reset());

  caors::indirect_view weak = weak_field.get_ref(&holder).create_indirect_view();
  REQUIRE(weak.valid());
  CHECK(weak.has_value());
  CHECK_FALSE(weak.value_ref().valid());
  CHECK(weak.reset());
  CHECK_FALSE(weak.has_value());
}
