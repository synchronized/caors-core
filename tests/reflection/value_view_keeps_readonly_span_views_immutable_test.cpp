// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "value_test_common.h"

using namespace caors::tests::reflection;

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
