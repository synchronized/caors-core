// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：resources_cache_loader_can_populate_cache_test.cpp

#include "resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - loader can populate cache", "[resources]") {
  caors::resource_cache<texture_resource> cache;
  counting_texture_loader loader;

  caors::resource_handle_result<texture_resource> loaded =
      caors::load_handle_into(cache, loader, test_guid_a);
  REQUIRE(loaded.succeeded());
  REQUIRE(loaded.value);

  CHECK(loader.load_count == 1);
  CHECK(loaded.value->guid() == test_guid_a);
  CHECK(cache.find(test_guid_a) == loaded.value.get());

  caors::resource_handle_result<texture_resource> cached =
      caors::load_handle_into(cache, loader, test_guid_a);
  REQUIRE(cached.succeeded());
  CHECK(cached.value.get() == loaded.value.get());
  CHECK(loader.load_count == 1);
}
