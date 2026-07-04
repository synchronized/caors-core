// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include <caors/value/view_factory.h>

#include <catch2/catch_all.hpp>

TEST_CASE("headers - value view factory exposes runtime view types", "[headers]") {
  caors::value::indirect_view indirect{};
  caors::value::sequence_view sequence{};
  caors::value::set_view set{};
  caors::value::associative_view associative{};
  caors::value::pair_view pair{};
  caors::value::tuple_view tuple{};
  caors::value::variant_view variant{};

  CHECK_FALSE(indirect.valid());
  CHECK_FALSE(sequence.valid());
  CHECK_FALSE(set.valid());
  CHECK_FALSE(associative.valid());
  CHECK_FALSE(pair.valid());
  CHECK_FALSE(tuple.valid());
  CHECK_FALSE(variant.valid());
}
