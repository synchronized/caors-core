// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - loader can populate cache", "[resources]") {
  caors::resource_cache<texture_resource> cache;
  counting_texture_loader loader;

  caors::result<texture_resource*> loaded = caors::load_into(cache, loader, test_guid_a);
  REQUIRE(loaded.succeeded());
  REQUIRE(loaded.value != nullptr);

  CHECK(loader.load_count == 1);
  CHECK(loaded.value->guid() == test_guid_a);
  CHECK(cache.find(test_guid_a) == loaded.value);

  caors::result<texture_resource*> cached = caors::load_into(cache, loader, test_guid_a);
  REQUIRE(cached.succeeded());
  CHECK(cached.value == loaded.value);
  CHECK(loader.load_count == 1);
}
