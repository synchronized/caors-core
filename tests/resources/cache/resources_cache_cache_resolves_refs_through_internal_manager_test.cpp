// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - cache resolves refs through internal manager", "[resources]") {
  caors::resource_cache<image_texture_resource> cache;
  image_texture_resource* image = cache.create(test_guid_a);
  REQUIRE(image != nullptr);

  caors::resource_ref<texture_resource> base_ref{test_guid_a};
  REQUIRE(cache.resolve(base_ref));

  CHECK(base_ref.resolved());
  CHECK(base_ref.get() == image);

  caors::resources::ref<image_texture_resource> derived_ref{test_guid_a};
  REQUIRE(cache.resolve(derived_ref));
  CHECK(derived_ref.get() == image);
}
