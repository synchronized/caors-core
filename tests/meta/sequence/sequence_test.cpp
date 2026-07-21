// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：meta_sequence_custom_sequence_traits_expose_sequence_view_test.cpp

#include "meta_property_test_support.h"

#include "caors/meta/value/associative_entry_ref.h"
#include "caors/meta/value/capabilities.h"

using namespace test_meta;

TEST_CASE("meta - custom sequence traits expose property view", "[meta]") {
  REQUIRE(
      caors::registration::type_<custom_sequence_holder>("caors.test.meta.custom_sequence_holder")
          .meta()
          .property("values", &custom_sequence_holder::values)
          .commit()
          .succeeded());

  auto reflected = caors::meta::find("caors.test.meta.custom_sequence_holder");
  REQUIRE(reflected);

  auto values = reflected.find_property("values");
  REQUIRE(values);
  CHECK(values.kind() == caors::meta::type_kind::sequence);
  CHECK(values.get_type().name() == "test.custom_sequence<int>");
  CHECK(values.get_element_type().id() == registered_type_id("int"));
  CHECK(values.get_element_type().name() == "int");

  custom_sequence_holder instance{};
  caors::meta::sequence_view sequence = values.get_ref(caors::meta::view(instance))
                                            .get_capability<caors::meta::sequence_capability>();
  CHECK(sequence.append_value(caors::any::from(21)));
  CHECK(sequence.append_value(caors::any::from(34)));

  const std::optional<std::size_t> size = sequence.size();
  REQUIRE(size.has_value());
  CHECK(*size == 2);

  caors::any_ref first = sequence.element_ref(0);
  REQUIRE(first.valid());
  const int* first_value = first.try_get<int>();
  REQUIRE(first_value != nullptr);
  CHECK(*first_value == 21);

  CHECK(sequence.resize(1));
  CHECK(instance.values.values.size() == 1);
  CHECK(sequence.clear());
  CHECK(instance.values.values.empty());
}

// 原测试文件：meta_sequence_fixed_std_span_field_name_includes_extent_test.cpp

#include "meta_property_test_support.h"

#include "caors/meta/value/associative_entry_ref.h"
#include "caors/meta/value/capabilities.h"

using namespace test_meta;

TEST_CASE("meta - fixed std span field name includes extent", "[meta]") {
  REQUIRE(caors::registration::type_<fixed_span_holder>("caors.test.meta.fixed_span_holder")
              .meta()
              .property("values", &fixed_span_holder::values)
              .commit()
              .succeeded());

  auto reflected = caors::meta::find("caors.test.meta.fixed_span_holder");
  REQUIRE(reflected);

  auto values = reflected.find_property("values");
  REQUIRE(values);
  CHECK(values.kind() == caors::meta::type_kind::sequence);
  CHECK(values.get_type().name() == "std.span<int,3>");
  CHECK(values.get_element_type().id() == registered_type_id("int"));
}

// 原测试文件：meta_sequence_list_view_uses_sequence_traits_test.cpp

#include "meta_property_test_support.h"

#include "caors/meta/value/associative_entry_ref.h"
#include "caors/meta/value/capabilities.h"

using namespace test_meta;

TEST_CASE("meta - list property view uses sequence traits", "[meta]") {
  REQUIRE(caors::registration::type_<list_holder>("caors.test.meta.list_holder")
              .meta()
              .property("values", &list_holder::values)
              .commit()
              .succeeded());

  auto reflected = caors::meta::find("caors.test.meta.list_holder");
  REQUIRE(reflected);

  auto values = reflected.find_property("values");
  REQUIRE(values);
  CHECK(values.kind() == caors::meta::type_kind::sequence);
  CHECK(values.get_type().name() == "caors.list<int>");
  CHECK(values.get_element_type().id() == registered_type_id("int"));

  list_holder instance{};
  instance.values.push_back(3);
  instance.values.push_back(5);

  caors::meta::sequence_view sequence = values.get_ref(caors::meta::view(instance))
                                            .get_capability<caors::meta::sequence_capability>();
  CHECK(sequence.valid());
  CHECK(sequence.writable());

  caors::any_ref second = sequence.element_ref(1);
  REQUIRE(second.valid());
  CHECK(second.try_set(8));

  auto iterator = instance.values.begin();
  std::advance(iterator, 1);
  CHECK(*iterator == 8);

  CHECK(sequence.append_value(caors::any::from(13)));
  CHECK(instance.values.back() == 13);
}

// 原测试文件：meta_sequence_sequence_view_exposes_element_metadata_test.cpp

#include "meta_property_test_support.h"

#include "caors/meta/value/associative_entry_ref.h"
#include "caors/meta/value/capabilities.h"

using namespace test_meta;

TEST_CASE("meta - sequence property view exposes element metadata", "[meta]") {
  REQUIRE(caors::registration::type_<vector_holder>("caors.test.meta.vector_holder")
              .meta()
              .property("values", &vector_holder::values)
              .commit()
              .succeeded());

  auto reflected = caors::meta::find("caors.test.meta.vector_holder");
  REQUIRE(reflected);

  auto values = reflected.find_property("values");
  REQUIRE(values);
  CHECK(values.kind() == caors::meta::type_kind::sequence);
  CHECK(values.is_sequence());
  CHECK(values.get_value_type().id() == registered_type_id("int"));
  CHECK(values.get_element_type().id() == registered_type_id("int"));
  CHECK(values.get_element_type().name() == "int");

  vector_holder instance{};
  instance.values.push_back(3);
  instance.values.push_back(5);

  caors::meta::sequence_view sequence = values.get_ref(caors::meta::view(instance))
                                            .get_capability<caors::meta::sequence_capability>();
  CHECK(sequence.valid());
  CHECK(sequence.writable());
  CHECK(sequence.get_value_type().id() == registered_type_id("int"));
  CHECK(sequence.get_value_type().name() == "int");
  CHECK(sequence.get_element_type().id() == registered_type_id("int"));
  CHECK(sequence.get_element_type().name() == "int");

  int sequence_sum = 0;
  for (caors::any_ref value : sequence) {
    const int* typed = value.try_get<int>();
    REQUIRE(typed != nullptr);
    sequence_sum += *typed;
  }
  CHECK(sequence_sum == 8);

  const std::optional<std::size_t> size = sequence.size();
  REQUIRE(size.has_value());
  CHECK(*size == 2);

  const vector_holder& readonly_instance = instance;
  caors::meta::sequence_view readonly_sequence =
      values.get_ref(caors::meta::view_const(readonly_instance))
          .get_capability<caors::meta::sequence_capability>();
  CHECK(readonly_sequence.valid());
  CHECK_FALSE(readonly_sequence.writable());

  caors::any_ref first = readonly_sequence.element_ref(0);
  REQUIRE(first.valid());
  CHECK_FALSE(first.writable());
  const int* first_value = first.try_get<int>();
  REQUIRE(first_value != nullptr);
  CHECK(*first_value == 3);

  caors::any_ref second = sequence.element_ref(1);
  REQUIRE(second.valid());
  CHECK(second.writable());
  CHECK(second.try_set<int>(8));
  CHECK(instance.values[1] == 8);

  CHECK(sequence.append_value(caors::any::from(13)));
  REQUIRE(instance.values.size() == 3);
  CHECK(instance.values[2] == 13);

  CHECK(sequence.resize(4));
  REQUIRE(instance.values.size() == 4);
  CHECK(instance.values[3] == 0);

  CHECK(sequence.clear());
  CHECK(instance.values.empty());
}

// 原测试文件：meta_sequence_std_array_view_uses_fixed_sequence_traits_test.cpp

#include "meta_property_test_support.h"

#include "caors/meta/value/associative_entry_ref.h"
#include "caors/meta/value/capabilities.h"

using namespace test_meta;

TEST_CASE("meta - std array property view uses fixed sequence traits", "[meta]") {
  REQUIRE(caors::registration::type_<array_holder>("caors.test.meta.array_holder")
              .meta()
              .property("values", &array_holder::values)
              .commit()
              .succeeded());

  auto reflected = caors::meta::find("caors.test.meta.array_holder");
  REQUIRE(reflected);

  auto values = reflected.find_property("values");
  REQUIRE(values);
  CHECK(values.kind() == caors::meta::type_kind::sequence);
  CHECK(values.get_type().name() == "std.array<int,3>");
  CHECK(values.get_element_type().id() == registered_type_id("int"));

  array_holder instance{{1, 2, 3}};
  caors::meta::sequence_view sequence = values.get_ref(caors::meta::view(instance))
                                            .get_capability<caors::meta::sequence_capability>();
  REQUIRE(sequence.valid());
  CHECK(sequence.writable());

  const std::optional<std::size_t> size = sequence.size();
  REQUIRE(size.has_value());
  CHECK(*size == 3);

  caors::any_ref second = sequence.element_ref(1);
  REQUIRE(second.valid());
  CHECK(second.try_set(8));
  CHECK(instance.values[1] == 8);

  CHECK(sequence.resize(3));
  CHECK_FALSE(sequence.resize(2));
  CHECK_FALSE(sequence.append_value(caors::any::from(13)));
}

// 原测试文件：meta_sequence_std_span_view_is_borrowed_sequence_test.cpp

#include "meta_property_test_support.h"

#include "caors/meta/value/associative_entry_ref.h"
#include "caors/meta/value/capabilities.h"

using namespace test_meta;

TEST_CASE("meta - std span property view is borrowed sequence", "[meta]") {
  REQUIRE(caors::registration::type_<span_holder>("caors.test.meta.span_holder")
              .meta()
              .property("values", &span_holder::values)
              .commit()
              .succeeded());

  auto reflected = caors::meta::find("caors.test.meta.span_holder");
  REQUIRE(reflected);

  auto values = reflected.find_property("values");
  REQUIRE(values);
  CHECK(values.kind() == caors::meta::type_kind::sequence);
  CHECK(values.get_type().name() == "std.span<int>");
  CHECK(values.get_element_type().id() == registered_type_id("int"));

  std::array<int, 3> storage{1, 2, 3};
  span_holder instance{std::span<int>{storage}};
  caors::meta::sequence_view sequence = values.get_ref(caors::meta::view(instance))
                                            .get_capability<caors::meta::sequence_capability>();
  REQUIRE(sequence.valid());
  CHECK(sequence.writable());

  const std::optional<std::size_t> size = sequence.size();
  REQUIRE(size.has_value());
  CHECK(*size == 3);

  caors::any_ref second = sequence.element_ref(1);
  REQUIRE(second.valid());
  CHECK(second.try_set(8));
  CHECK(storage[1] == 8);

  CHECK(sequence.resize(3));
  CHECK_FALSE(sequence.resize(2));
  CHECK_FALSE(sequence.append_value(caors::any::from(13)));
}
