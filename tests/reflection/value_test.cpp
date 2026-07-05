// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/adapters/stl.h"
#include "caors/memory.h"
#include "caors/reflection.h"
#include "caors/value/view_factory.h"

#include <catch2/catch_all.hpp>

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace {

struct reflected_value_holder {
  int value = 0;
  const int readonly = 7;
};

struct move_only_value {
  std::unique_ptr<int> value{};
};

struct any_resource_value {
  int value = 17;
};

struct unregistered_nested_vector_holder {
  std::vector<std::vector<int>> values{};
};

struct indirect_value_holder {
  int value = 7;
  int* pointer = &value;
  std::optional<int> optional = 11;
  std::unique_ptr<int> unique = std::make_unique<int>(13);
  std::weak_ptr<int> weak{};
};

[[nodiscard]] bool type_result_ok(caors::error result) noexcept {
  return result.succeeded() || result.value() == caors::errc::duplicate_id;
}

template <typename T> void require_registered_type(std::string_view name) {
  REQUIRE(type_result_ok(caors::meta::type_<T>(name).result()));
}

void require_value_test_meta() {
  require_registered_type<int>("int");
  require_registered_type<float>("float");
  require_registered_type<caors::string>("caors.string");
  require_registered_type<std::unique_ptr<int>>("std.unique_ptr<int>");
  require_registered_type<std::weak_ptr<int>>("std.weak_ptr<int>");
  require_registered_type<std::optional<int>>("std.optional<int>");
  require_registered_type<std::pair<int, int>>("std.pair<int,int>");
  require_registered_type<std::tuple<int, int, caors::string>>("std.tuple<int,int,caors.string>");
  require_registered_type<std::span<int>>("std.span<int>");
  require_registered_type<std::variant<int, caors::string>>("std.variant<int,caors.string>");
  require_registered_type<any_resource_value>("caors.test.reflection.any_resource_value");
}

[[nodiscard]] caors::reflection::field require_reflected_field(std::string_view type_name,
                                                               std::string_view field_name) {
  auto reflected = caors::reflection::find(type_name);
  REQUIRE(reflected);
  auto field = reflected.find_field(field_name);
  REQUIRE(field);
  return field;
}

} // namespace

TEST_CASE("reflection any - stores copied values", "[reflection]") {
  require_value_test_meta();

  int source = 7;
  caors::any stored = caors::any::from(source);

  CHECK(stored.valid());
  const auto int_type = caors::meta::find("int");
  REQUIRE(int_type);
  CHECK(stored.type() == int_type.id());
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
  require_value_test_meta();

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

TEST_CASE("reflection any - default construction can use memory resource", "[reflection]") {
  require_value_test_meta();

  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  caors::any stored = caors::any::default_construct(
      caors::meta::id("caors.test.reflection.any_resource_value"), &tracking);

  REQUIRE(stored.valid());
  REQUIRE(stored.try_get<any_resource_value>() != nullptr);
  CHECK(stored.try_get<any_resource_value>()->value == 17);
  CHECK(tracking.statistics().allocation_count == 1);
  CHECK(tracking.statistics().bytes_in_use >= sizeof(any_resource_value));

  stored.reset();
  CHECK(tracking.statistics().deallocation_count == 1);
  CHECK(tracking.statistics().bytes_in_use == 0);
}

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

TEST_CASE("reflection any - descriptor fields do not require container type registration",
          "[reflection]") {
  require_value_test_meta();

  REQUIRE(caors::reflection::class_<unregistered_nested_vector_holder>(
              "caors.test.reflection.unregistered_nested_vector_holder")
              .field("values", &unregistered_nested_vector_holder::values)
              .commit() == caors::error{});

  auto reflected =
      caors::reflection::find("caors.test.reflection.unregistered_nested_vector_holder");
  REQUIRE(reflected);
  auto values_field = reflected.find_field("values");
  REQUIRE(values_field);
  CHECK(values_field.type_name().empty());
  CHECK(values_field.size() == sizeof(std::vector<std::vector<int>>));
  CHECK(values_field.alignment() == alignof(std::vector<std::vector<int>>));
  CHECK(values_field.kind() == caors::meta::type_kind::sequence);
  CHECK(static_cast<bool>(values_field.value_type()));
  CHECK_FALSE(caors::meta::find(values_field.value_type()));

  unregistered_nested_vector_holder holder{{{1, 2}, {3}}};
  caors::value::sequence_view outer = values_field.get_ref(&holder).create_sequence_view();
  REQUIRE(outer.valid());
  REQUIRE(outer.size().has_value());
  CHECK(*outer.size() == 2);

  caors::value::sequence_view inner = outer.element_ref(0).create_sequence_view();
  REQUIRE(inner.valid());
  CHECK(inner.element_type() == caors::meta::id("int"));
  CHECK(inner.element_ref(0).try_set(5));
  CHECK(holder.values[0][0] == 5);
}

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
  CHECK(pointer.value_type() == caors::meta::id("int"));
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

TEST_CASE("reflection any_ref - creates indirect views for registered wrapper values",
          "[reflection]") {
  require_value_test_meta();

  std::optional<int> value = 41;

  caors::any_ref reference = caors::any_ref::from(value, "std.optional<int>");
  REQUIRE(reference.valid());

  caors::value::indirect_view indirect = reference.create_indirect_view();
  REQUIRE(indirect.valid());
  CHECK(indirect.has_value());
  CHECK(indirect.value_type() == caors::meta::id("int"));

  caors::any_ref inner = indirect.value_ref();
  REQUIRE(inner.valid());
  CHECK(inner.try_set(43));
  CHECK(value == 43);

  CHECK(indirect.reset());
  CHECK_FALSE(value.has_value());
  CHECK_FALSE(indirect.has_value());
}

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

TEST_CASE("reflection any_ref - creates tuple views for registered tuple values", "[reflection]") {
  require_value_test_meta();

  std::tuple<int, int, caors::string> value{3, 5, caors::string{"five"}};

  caors::any_ref reference = caors::any_ref::from(value, "std.tuple<int,int,caors.string>");
  REQUIRE(reference.valid());

  caors::value::tuple_view tuple = reference.create_tuple_view();
  REQUIRE(tuple.valid());
  CHECK(tuple.writable());
  CHECK(tuple.size() == 3);
  REQUIRE(tuple.element_type(0).has_value());
  CHECK(*tuple.element_type(0) == caors::meta::id("int"));
  CHECK(tuple.element_type_name(2) == "caors.string");

  caors::any_ref first = tuple.element_ref(0);
  caors::any_ref second = tuple.element_ref(1);
  caors::any_ref third = tuple.element_ref(2);
  REQUIRE(first.valid());
  REQUIRE(second.valid());
  REQUIRE(third.valid());

  CHECK(first.try_set(7));
  CHECK(second.try_set(11));
  CHECK(third.try_set(caors::string{"eleven"}));
  CHECK(std::get<0>(value) == 7);
  CHECK(std::get<1>(value) == 11);
  CHECK(std::get<2>(value) == "eleven");
}

TEST_CASE("reflection any_ref - creates sequence views for registered span values",
          "[reflection]") {
  require_value_test_meta();

  std::array<int, 3> storage{3, 5, 8};
  std::span<int> value{storage};

  caors::any_ref reference = caors::any_ref::from(value, "std.span<int>");
  REQUIRE(reference.valid());

  caors::value::sequence_view sequence = reference.create_sequence_view();
  REQUIRE(sequence.valid());
  CHECK(sequence.writable());
  CHECK(sequence.value_type() == caors::meta::id("int"));

  const std::optional<std::size_t> size = sequence.size();
  REQUIRE(size.has_value());
  CHECK(*size == 3);

  caors::any_ref second = sequence.element_ref(1);
  REQUIRE(second.valid());
  CHECK(second.try_set(13));
  CHECK(storage[1] == 13);
  CHECK(sequence.resize(3));
  CHECK_FALSE(sequence.resize(2));
}

TEST_CASE("reflection any_ref - keeps readonly span views immutable", "[reflection]") {
  require_value_test_meta();

  std::array<int, 3> storage{3, 5, 8};
  std::span<int> value{storage};

  caors::any_ref reference = caors::any_ref::from_const(value, "std.span<int>");
  REQUIRE(reference.valid());
  CHECK_FALSE(reference.writable());

  caors::value::sequence_view sequence = reference.create_sequence_view();
  REQUIRE(sequence.valid());
  CHECK_FALSE(sequence.writable());

  caors::any_ref second = sequence.element_ref(1);
  REQUIRE(second.valid());
  CHECK_FALSE(second.writable());
  CHECK_FALSE(second.try_set(13));
  CHECK(storage[1] == 5);
}

TEST_CASE("reflection any_ref - creates variant views for registered variant values",
          "[reflection]") {
  require_value_test_meta();

  std::variant<int, caors::string> value{3};

  caors::any_ref reference = caors::any_ref::from(value, "std.variant<int,caors.string>");
  REQUIRE(reference.valid());

  caors::value::variant_view variant = reference.create_variant_view();
  REQUIRE(variant.valid());
  CHECK(variant.writable());
  CHECK(variant.size() == 2);
  REQUIRE(variant.index().has_value());
  CHECK(*variant.index() == 0);
  REQUIRE(variant.alternative_type(1).has_value());
  CHECK(*variant.alternative_type(1) == caors::meta::id("caors.string"));

  caors::any_ref active = variant.value_ref();
  REQUIRE(active.valid());
  CHECK(active.try_set(7));
  CHECK(std::get<int>(value) == 7);

  REQUIRE(variant.emplace(1));
  CHECK(variant.value_type_name() == "caors.string");
  active = variant.value_ref();
  REQUIRE(active.valid());
  CHECK(active.try_set(caors::string{"seven"}));
  CHECK(std::get<caors::string>(value) == "seven");
}
