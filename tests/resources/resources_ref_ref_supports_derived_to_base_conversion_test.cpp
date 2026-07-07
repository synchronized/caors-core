// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - ref supports derived to base conversion", "[resources]") {
  image_texture_resource image{test_guid_a};
  caors::resources::ref<image_texture_resource> derived{&image};
  caors::resource_ref<texture_resource> base{derived};

  CHECK(base.resolved());
  CHECK(base.get() == &image);
  CHECK(base.guid() == test_guid_a);

  base.reset();
  CHECK_FALSE(base);
  CHECK_FALSE(base.resolved());
  CHECK(base.guid() == caors::resource_guid::nil());
}
