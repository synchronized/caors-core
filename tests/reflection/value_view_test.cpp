// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "value_test_common.h"

using namespace caors::tests::reflection;

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
