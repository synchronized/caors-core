// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：value_any_copies_deeply_and_rejects_mismatched_writes_test.cpp

#include "value_test_common.h"

using namespace caors::tests::reflection;

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

// 原测试文件：value_any_default_construction_can_use_memory_resource_test.cpp

#include "value_test_common.h"

using namespace caors::tests::reflection;

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

// 原测试文件：value_any_default_constructs_small_trivial_values_inline_test.cpp

#include "value_test_common.h"

using namespace caors::tests::reflection;

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

// 原测试文件：value_any_fields_expose_copied_runtime_values_test.cpp

#include "value_test_common.h"

using namespace caors::tests::reflection;

TEST_CASE("reflection any - fields expose copied runtime values", "[reflection]") {
  require_value_test_meta();

  REQUIRE(((void)caors::meta::class_<reflected_value_holder>(
               "caors.test.reflection.value_reflected_holder"),
           caors::reflection::class_<reflected_value_holder>())
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

// 原测试文件：value_any_keeps_large_trivial_values_on_heap_test.cpp

#include "value_test_common.h"

using namespace caors::tests::reflection;

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

// 原测试文件：value_any_noncopyable_fields_return_invalid_values_test.cpp

#include "value_test_common.h"

using namespace caors::tests::reflection;

TEST_CASE("reflection any - noncopyable fields return invalid values", "[reflection]") {
  require_value_test_meta();

  REQUIRE(((void)caors::meta::class_<move_only_value>("caors.test.reflection.move_only_value"),
           caors::reflection::class_<move_only_value>())
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

// 原测试文件：value_any_ref_creates_descriptor_views_for_unregistered_nested_vectors_test.cpp

#include "value_test_common.h"

using namespace caors::tests::reflection;

TEST_CASE("reflection any_ref - creates descriptor views for unregistered nested vectors",
          "[reflection]") {
  require_value_test_meta();

  std::vector<std::vector<int>> values{{1, 2}, {3}};

  caors::any_ref reference = caors::any_ref::from(values);
  REQUIRE(reference.valid());
  CHECK(reference.get_type().name() == "std.vector<std.vector<int>>");

  caors::meta::sequence_view outer = reference.get_capability<caors::meta::sequence_capability>();
  REQUIRE(outer.valid());
  CHECK(outer.writable());
  CHECK(static_cast<bool>(outer.get_value_type().id()));
  CHECK_FALSE(caors::meta::find(outer.get_value_type().id()).registered());
  CHECK(outer.get_value_type());
  CHECK_FALSE(outer.get_value_type().registered());
  CHECK(outer.get_value_type().name() == "std.vector<int>");

  caors::any_ref first_row = outer.element_ref(0);
  REQUIRE(first_row.valid());
  caors::meta::sequence_view inner = first_row.get_capability<caors::meta::sequence_capability>();
  REQUIRE(inner.valid());
  CHECK(inner.get_value_type().id() == caors::any::from(0).get_type().id());

  caors::any_ref second = inner.element_ref(1);
  REQUIRE(second.valid());
  CHECK(second.try_set(8));
  CHECK(values[0][1] == 8);

  CHECK(outer.append_value(caors::any::from(std::vector<int>{13, 21})));
  REQUIRE(values.size() == 3);
  CHECK(values[2][0] == 13);
}

// 原测试文件：value_any_ref_descriptor_fields_do_not_require_container_type_registration_test.cpp

#include "value_test_common.h"

using namespace caors::tests::reflection;

TEST_CASE("reflection any - descriptor fields do not require container type registration",
          "[reflection]") {
  require_value_test_meta();

  REQUIRE(((void)caors::meta::class_<unregistered_nested_vector_holder>(
               "caors.test.reflection.unregistered_nested_vector_holder"),
           caors::reflection::class_<unregistered_nested_vector_holder>())
              .field("values", &unregistered_nested_vector_holder::values)
              .commit() == caors::error{});

  auto reflected =
      caors::reflection::find("caors.test.reflection.unregistered_nested_vector_holder");
  REQUIRE(reflected);
  auto values_field = reflected.find_field("values");
  REQUIRE(values_field);
  CHECK(values_field.get_type().name() == "std.vector<std.vector<int>>");
  CHECK(values_field.size() == sizeof(std::vector<std::vector<int>>));
  CHECK(values_field.alignment() == alignof(std::vector<std::vector<int>>));
  CHECK(values_field.kind() == caors::meta::type_kind::sequence);
  CHECK(static_cast<bool>(values_field.get_value_type().id()));
  CHECK_FALSE(caors::meta::find(values_field.get_value_type().id()).registered());
  CHECK(values_field.get_type());
  CHECK_FALSE(values_field.get_type().registered());
  CHECK(values_field.get_value_type());
  CHECK_FALSE(values_field.get_value_type().registered());

  unregistered_nested_vector_holder holder{{{1, 2}, {3}}};
  caors::meta::sequence_view outer =
      values_field.get_ref(&holder).get_capability<caors::meta::sequence_capability>();
  REQUIRE(outer.valid());
  REQUIRE(outer.size().has_value());
  CHECK(*outer.size() == 2);

  caors::meta::sequence_view inner =
      outer.element_ref(0).get_capability<caors::meta::sequence_capability>();
  REQUIRE(inner.valid());
  CHECK(inner.get_element_type().id() == caors::any::from(0).get_type().id());
  CHECK(inner.element_ref(0).try_set(5));
  CHECK(holder.values[0][0] == 5);
}

// 原测试文件：value_any_ref_references_external_typed_values_test.cpp

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

// 原测试文件：value_any_stores_copied_values_test.cpp

#include "value_test_common.h"

using namespace caors::tests::reflection;

TEST_CASE("reflection any - stores copied values", "[reflection]") {
  require_value_test_meta();

  int source = 7;
  caors::any stored = caors::any::from(source);

  CHECK(stored.valid());
  CHECK(stored.get_type().id() == caors::any::from(0).get_type().id());
  CHECK(stored.get_type().name() == "int");
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

// 原测试文件：value_any_stores_small_trivial_values_inline_test.cpp

#include "value_test_common.h"

using namespace caors::tests::reflection;

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
