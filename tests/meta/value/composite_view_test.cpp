// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：value_view_creates_pair_views_for_registered_pair_values_test.cpp

#include "meta_value_test_common.h"

using namespace caors::tests::reflection;

TEST_CASE("meta any_ref - creates pair views from descriptor values", "[meta]") {
  require_value_test_meta();

  std::pair<int, int> value{3, 5};

  caors::any_ref reference = caors::any_ref::from(value);
  REQUIRE(reference.valid());

  caors::meta::pair_view pair = reference.get_capability<caors::meta::pair_capability>();
  REQUIRE(pair.valid());
  CHECK(pair.writable());
  const caors::type_id int_type = caors::any::from(0).get_type().id();
  CHECK(pair.get_first_type().id() == int_type);
  CHECK(pair.get_second_type().id() == int_type);
  CHECK(pair.get_first_type().id() == int_type);
  CHECK(pair.get_second_type().id() == int_type);

  caors::any_ref first = pair.first_ref();
  caors::any_ref second = pair.second_ref();
  REQUIRE(first.valid());
  REQUIRE(second.valid());

  CHECK(first.try_set(7));
  CHECK(second.try_set(11));
  CHECK(value.first == 7);
  CHECK(value.second == 11);
}

// 原测试文件：value_view_creates_sequence_views_for_registered_span_values_test.cpp

#include "meta_value_test_common.h"

using namespace caors::tests::reflection;

TEST_CASE("meta any_ref - creates sequence views from descriptor span values", "[meta]") {
  require_value_test_meta();

  std::array<int, 3> storage{3, 5, 8};
  std::span<int> value{storage};

  caors::any_ref reference = caors::any_ref::from(value);
  REQUIRE(reference.valid());

  caors::meta::sequence_view sequence =
      reference.get_capability<caors::meta::sequence_capability>();
  REQUIRE(sequence.valid());
  CHECK(sequence.writable());
  CHECK(sequence.get_value_type().id() == caors::any::from(0).get_type().id());
  CHECK(sequence.get_value_type().id() == caors::any::from(0).get_type().id());

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

// 原测试文件：value_view_creates_tuple_views_for_registered_tuple_values_test.cpp

#include "meta_value_test_common.h"

using namespace caors::tests::reflection;

TEST_CASE("meta any_ref - creates tuple views from descriptor values", "[meta]") {
  require_value_test_meta();

  std::tuple<int, int, caors::string> value{3, 5, caors::string{"five"}};

  caors::any_ref reference = caors::any_ref::from(value);
  REQUIRE(reference.valid());

  caors::meta::tuple_view tuple = reference.get_capability<caors::meta::tuple_capability>();
  REQUIRE(tuple.valid());
  CHECK(tuple.writable());
  CHECK(tuple.size() == 3);
  REQUIRE(tuple.get_element_type(0).has_value());
  CHECK(tuple.get_element_type(0)->id() == caors::any::from(0).get_type().id());
  CHECK(tuple.get_element_type(2)->name() == "caors.string");

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

// 原测试文件：value_view_creates_variant_views_for_registered_variant_values_test.cpp

#include "meta_value_test_common.h"

using namespace caors::tests::reflection;

TEST_CASE("meta any_ref - creates variant views from descriptor values", "[meta]") {
  require_value_test_meta();

  std::variant<int, caors::string> value{3};

  caors::any_ref reference = caors::any_ref::from(value);
  REQUIRE(reference.valid());

  caors::meta::variant_view variant = reference.get_capability<caors::meta::variant_capability>();
  REQUIRE(variant.valid());
  CHECK(variant.writable());
  CHECK(variant.size() == 2);
  REQUIRE(variant.index().has_value());
  CHECK(*variant.index() == 0);
  REQUIRE(variant.get_alternative_type(1).has_value());
  CHECK(variant.get_alternative_type(1)->id() == caors::any::from(caors::string{}).get_type().id());

  caors::any_ref active = variant.value_ref();
  REQUIRE(active.valid());
  CHECK(active.try_set(7));
  CHECK(std::get<int>(value) == 7);

  REQUIRE(variant.emplace(1));
  CHECK(variant.get_value_type().id() == caors::any::from(caors::string{}).get_type().id());
  CHECK(variant.get_value_type().name() == "caors.string");
  active = variant.value_ref();
  REQUIRE(active.valid());
  CHECK(active.try_set(caors::string{"seven"}));
  CHECK(std::get<caors::string>(value) == "seven");
}

// 原测试文件：value_view_keeps_readonly_span_views_immutable_test.cpp

#include "meta_value_test_common.h"

using namespace caors::tests::reflection;

TEST_CASE("meta any_ref - keeps readonly span views immutable", "[meta]") {
  require_value_test_meta();

  std::array<int, 3> storage{3, 5, 8};
  std::span<int> value{storage};

  caors::any_ref reference = caors::any_ref::from_const(value);
  REQUIRE(reference.valid());
  CHECK_FALSE(reference.writable());

  caors::meta::sequence_view sequence =
      reference.get_capability<caors::meta::sequence_capability>();
  REQUIRE(sequence.valid());
  CHECK_FALSE(sequence.writable());

  caors::any_ref second = sequence.element_ref(1);
  REQUIRE(second.valid());
  CHECK_FALSE(second.writable());
  CHECK_FALSE(second.try_set(13));
  CHECK(storage[1] == 5);
}
