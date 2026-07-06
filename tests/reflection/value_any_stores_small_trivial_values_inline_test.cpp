// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

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
