// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - cache inserts owned resources", "[resources]") {
  caors::resource_cache<texture_resource> cache;
  auto texture = caors::memory::make_unique<texture_resource>(nullptr, test_guid_a);
  texture_resource* stored = texture.get();

  REQUIRE(cache.insert(std::move(texture)));

  CHECK(cache.size() == 1);
  CHECK(cache.find(test_guid_a) == stored);
  CHECK(cache.index().find(test_guid_a) == stored);
  CHECK_FALSE(cache.insert({}));
}
