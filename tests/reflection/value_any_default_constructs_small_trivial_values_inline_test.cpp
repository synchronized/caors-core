// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

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
