// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - cache rejects invalid duplicate and mismatched resources", "[resources]") {
  caors::resource_cache<texture_resource> cache;

  CHECK(cache.create(caors::resource_guid::nil()) == nullptr);
  REQUIRE(cache.create(test_guid_a) != nullptr);

  CHECK(cache.create(test_guid_a) == nullptr);
  CHECK(cache.create(test_guid_b, test_guid_a) == nullptr);
  CHECK_FALSE(cache.contains(test_guid_b));
}
