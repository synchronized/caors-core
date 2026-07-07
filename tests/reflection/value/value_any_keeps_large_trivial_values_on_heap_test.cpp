// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "value_test_common.h"

using namespace caors::tests::reflection;

TEST_CASE("reflection any - keeps large trivial values on heap", "[reflection]") {
  tracked_large_trivial::allocation_count = 0;
  tracked_large_trivial::deallocation_count = 0;

  tracked_large_trivial source{};
  source.value = 64;

  caors::any stored = caors::any::from(source);
  REQUIRE(stored.valid());
  REQUIRE(stored.try_get<tracked_large_trivial>() != nullptr);
  CHECK(stored.try_get<tracked_large_trivial>()->value == 64);
  CHECK(tracked_large_trivial::allocation_count == 1);

  {
    caors::any copied = stored;
    REQUIRE(copied.try_get<tracked_large_trivial>() != nullptr);
    CHECK(copied.try_get<tracked_large_trivial>()->value == 64);
    CHECK(tracked_large_trivial::allocation_count == 2);
  }

  CHECK(tracked_large_trivial::deallocation_count == 1);

  stored.reset();
  CHECK(tracked_large_trivial::allocation_count == 2);
  CHECK(tracked_large_trivial::deallocation_count == 2);
}
