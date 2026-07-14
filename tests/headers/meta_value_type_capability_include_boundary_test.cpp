// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include <caors/adapters/stl_traits.h>
#include <caors/meta/value/any_ref.h>
#include <caors/meta/value/type_capability.h>

#include <catch2/catch_all.hpp>

#include <vector>

TEST_CASE("headers - meta value type capability exposes type facade", "[headers]") {
  std::vector<int> values{1, 2, 3};

  caors::meta::sequence_capability sequence =
      caors::meta::resolve<std::vector<int>>().get_capability<caors::meta::sequence_capability>();
  REQUIRE(sequence);

  caors::meta::sequence_view view = sequence.bind(caors::meta::any_ref::from(values));
  REQUIRE(view);
  REQUIRE(view.size().has_value());
  CHECK(*view.size() == 3);
}
