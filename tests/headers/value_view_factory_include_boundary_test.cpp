// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include <caors/meta/value/view_factory.h>

#include <catch2/catch_all.hpp>

TEST_CASE("headers - meta value view factory exposes runtime view types", "[headers]") {
  caors::meta::indirect_view indirect{};
  caors::meta::sequence_view sequence{};
  caors::meta::set_view set{};
  caors::meta::associative_view associative{};
  caors::meta::pair_view pair{};
  caors::meta::tuple_view tuple{};
  caors::meta::variant_view variant{};

  CHECK_FALSE(indirect.valid());
  CHECK_FALSE(sequence.valid());
  CHECK_FALSE(set.valid());
  CHECK_FALSE(associative.valid());
  CHECK_FALSE(pair.valid());
  CHECK_FALSE(tuple.valid());
  CHECK_FALSE(variant.valid());
}
