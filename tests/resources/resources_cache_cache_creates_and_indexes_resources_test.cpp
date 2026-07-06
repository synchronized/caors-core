// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - cache creates and indexes resources", "[resources]") {
  caors::resource_cache<texture_resource> cache;

  texture_resource* texture = cache.create(test_guid_a);
  REQUIRE(texture != nullptr);

  CHECK(cache.size() == 1);
  CHECK(cache.contains(test_guid_a));
  CHECK(texture->guid() == test_guid_a);
  CHECK(cache.find(test_guid_a) == texture);
  CHECK(cache.find_as<texture_resource>(test_guid_a) == texture);
  CHECK(cache.index().find(test_guid_a) == texture);
}
