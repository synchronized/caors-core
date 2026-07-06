// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "value_test_common.h"

using namespace caors::tests::reflection;

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

TEST_CASE("reflection any - stores small trivial values inline", "[reflection]") {
  tracked_small_trivial::allocation_count = 0;
  tracked_small_trivial::deallocation_count = 0;

  tracked_small_trivial source{42};
  caors::any stored = caors::any::from(source);

  REQUIRE(stored.valid());
  REQUIRE(stored.try_get<tracked_small_trivial>() != nullptr);
  CHECK(stored.try_get<tracked_small_trivial>()->value == 42);
  CHECK(tracked_small_trivial::allocation_count == 0);

  caors::any copied = stored;
  REQUIRE(copied.try_get<tracked_small_trivial>() != nullptr);
  CHECK(copied.try_get<tracked_small_trivial>()->value == 42);
  CHECK(tracked_small_trivial::allocation_count == 0);

  REQUIRE(stored.try_set(tracked_small_trivial{77}));
  CHECK(stored.try_get<tracked_small_trivial>()->value == 77);
  CHECK(copied.try_get<tracked_small_trivial>()->value == 42);

  caors::any moved = std::move(stored);
  CHECK_FALSE(stored.valid());
  REQUIRE(moved.try_get<tracked_small_trivial>() != nullptr);
  CHECK(moved.try_get<tracked_small_trivial>()->value == 77);

  moved.reset();
  copied.reset();
  CHECK(tracked_small_trivial::allocation_count == 0);
  CHECK(tracked_small_trivial::deallocation_count == 0);
}

TEST_CASE("reflection any - default constructs small trivial values inline", "[reflection]") {
  require_registered_type<tracked_small_trivial>("caors.test.reflection.tracked_small_trivial");
  tracked_small_trivial::allocation_count = 0;
  tracked_small_trivial::deallocation_count = 0;

  caors::any stored =
      caors::any::default_construct(caors::meta::id("caors.test.reflection.tracked_small_trivial"));

  REQUIRE(stored.valid());
  REQUIRE(stored.try_get<tracked_small_trivial>() != nullptr);
  CHECK(stored.try_get<tracked_small_trivial>()->value == 0);
  CHECK(tracked_small_trivial::allocation_count == 0);

  stored.reset();
  CHECK(tracked_small_trivial::allocation_count == 0);
  CHECK(tracked_small_trivial::deallocation_count == 0);
}

TEST_CASE("reflection any - keeps large trivial values on heap", "[reflection]") {
  tracked_large_trivial::allocation_count = 0;
  tracked_large_trivial::deallocation_count = 0;

  tracked_large_trivial source{};
  source.value = 64;

  caors::any stored = caors::any::from(source);
  REQUIRE(stored.valid());
  REQUIRE(stored.try_get<tracked_large_trivial>() != nullptr);
  CHECK(stored.try_get<tracked_large_trivial>()->value == 64);
  CHECK(tracked_large_trivial::allocation_count == 1);

  {
    caors::any copied = stored;
    REQUIRE(copied.try_get<tracked_large_trivial>() != nullptr);
    CHECK(copied.try_get<tracked_large_trivial>()->value == 64);
    CHECK(tracked_large_trivial::allocation_count == 2);
  }

  CHECK(tracked_large_trivial::deallocation_count == 1);

  stored.reset();
  CHECK(tracked_large_trivial::allocation_count == 2);
  CHECK(tracked_large_trivial::deallocation_count == 2);
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
