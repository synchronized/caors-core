// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/reflection.h"

#include <catch2/catch_all.hpp>

#include <cstddef>
#include <memory>
#include <utility>

namespace {

struct reflected_value_holder {
  int value = 0;
  const int readonly = 7;
};

struct move_only_value {
  std::unique_ptr<int> value{};
};

} // namespace

TEST_CASE("reflection value - stores copied values", "[reflection]") {
  int source = 7;
  caors::value stored = caors::value::from(source);

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

TEST_CASE("reflection value - copies deeply and rejects mismatched writes", "[reflection]") {
  caors::value stored = caors::value::from(7);
  caors::value copied = stored;

  REQUIRE(stored.try_set(8));
  REQUIRE(copied.try_get<int>() != nullptr);
  CHECK(*copied.try_get<int>() == 7);
  CHECK(*stored.try_get<int>() == 8);

  float other = 1.0F;
  CHECK_FALSE(stored.matches_type<float>());
  CHECK_FALSE(stored.try_set(caors::value::from(other)));
  CHECK(*stored.try_get<int>() == 8);

  caors::value moved = std::move(stored);
  CHECK(moved.valid());
  CHECK_FALSE(stored.valid());
  CHECK(*moved.try_get<int>() == 8);
}

TEST_CASE("reflection value - fields expose copied runtime values", "[reflection]") {
  reflected_value_holder holder{1, 7};
  const caors::reflection_field_data value = caors::make_reflection_field_data<int>(
      "value", "int", offsetof(reflected_value_holder, value));
  const caors::reflection_field_data readonly = caors::make_reflection_field_data<const int>(
      "readonly", "int", offsetof(reflected_value_holder, readonly));

  caors::value copied = value.get_value(&holder);
  CHECK(copied.valid());
  CHECK(copied.matches_type<int>());
  REQUIRE(copied.try_get<int>() != nullptr);
  CHECK(*copied.try_get<int>() == 1);

  CHECK(copied.try_set(10));
  CHECK(*copied.try_get<int>() == 10);
  CHECK(holder.value == 1);

  CHECK(value.try_set_value(&holder, copied));
  CHECK(holder.value == 10);

  caors::value mismatch = caors::value::from(2.0F);
  CHECK_FALSE(value.try_set_value(&holder, mismatch));
  CHECK(holder.value == 10);

  caors::value readonly_value = readonly.get_value(&holder);
  CHECK(readonly_value.valid());
  REQUIRE(readonly_value.try_get<int>() != nullptr);
  CHECK(*readonly_value.try_get<int>() == 7);
  CHECK(readonly_value.try_set(8));
  CHECK(*readonly_value.try_get<int>() == 8);
  CHECK_FALSE(readonly.try_set_value(&holder, readonly_value));
  CHECK(holder.readonly == 7);
}

TEST_CASE("reflection value - noncopyable fields return invalid values", "[reflection]") {
  move_only_value holder{std::make_unique<int>(7)};
  const caors::reflection_field_data field =
      caors::make_reflection_field_data<std::unique_ptr<int>>("value", "std.unique_ptr<int>",
                                                              offsetof(move_only_value, value));

  caors::value copied = field.get_value(&holder);
  CHECK_FALSE(copied.valid());
}
