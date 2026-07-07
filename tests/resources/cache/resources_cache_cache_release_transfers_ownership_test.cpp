// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - cache release transfers ownership", "[resources]") {
  observed_resource::destroyed = 0;

  caors::resource_cache<observed_resource> cache;
  REQUIRE(cache.create(test_guid_a) != nullptr);

  auto released = cache.release(test_guid_a);

  REQUIRE(released != nullptr);
  CHECK(cache.empty());
  CHECK(cache.index().find(test_guid_a) == nullptr);
  CHECK(released->guid() == test_guid_a);
  CHECK(observed_resource::destroyed == 0);

  released.reset();
  CHECK(observed_resource::destroyed == 1);
}
