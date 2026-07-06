// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - cache uses requested memory resource", "[resources]") {
  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  caors::resource_cache<texture_resource> cache{&tracking};

  REQUIRE(cache.create(test_guid_a) != nullptr);

  CHECK(cache.resource() == &tracking);
  CHECK(cache.index().resource() == &tracking);
  CHECK(tracking.statistics().allocation_count > 0);
}
