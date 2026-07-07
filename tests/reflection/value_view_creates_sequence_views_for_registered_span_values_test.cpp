// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "value_test_common.h"

using namespace caors::tests::reflection;

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
