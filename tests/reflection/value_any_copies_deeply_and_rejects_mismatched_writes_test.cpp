// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "value_test_common.h"

using namespace caors::tests::reflection;

TEST_CASE("reflection any - copies deeply and rejects mismatched writes", "[reflection]") {
  require_value_test_meta();

  caors::any stored = caors::any::from(7);
  caors::any copied = stored;

  REQUIRE(stored.try_set(8));
  REQUIRE(copied.try_get<int>() != nullptr);
  CHECK(*copied.try_get<int>() == 7);
  CHECK(*stored.try_get<int>() == 8);

  float other = 1.0F;
  CHECK_FALSE(stored.matches_type<float>());
  CHECK_FALSE(stored.try_set(caors::any::from(other)));
  CHECK(*stored.try_get<int>() == 8);

  caors::any moved = std::move(stored);
  CHECK(moved.valid());
  CHECK_FALSE(stored.valid());
  CHECK(*moved.try_get<int>() == 8);
}
