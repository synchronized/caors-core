// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - cache erase destroys owned resource", "[resources]") {
  observed_resource::destroyed = 0;

  caors::resource_cache<observed_resource> cache;
  REQUIRE(cache.create(test_guid_a) != nullptr);

  CHECK(cache.erase(test_guid_a));

  CHECK(observed_resource::destroyed == 1);
  CHECK(cache.empty());
  CHECK(cache.index().find(test_guid_a) == nullptr);
  CHECK_FALSE(cache.erase(test_guid_a));
}
