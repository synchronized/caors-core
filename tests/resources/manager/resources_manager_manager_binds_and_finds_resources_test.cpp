// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - manager binds and finds resources", "[resources]") {
  caors::resource_manager manager;
  texture_resource texture{test_guid_a};

  CHECK(manager.empty());
  CHECK(manager.resource() != nullptr);

  REQUIRE(manager.bind(&texture));

  CHECK(manager.size() == 1);
  CHECK(manager.contains(test_guid_a));
  CHECK(manager.contains(&texture));
  CHECK(manager.find(test_guid_a) == &texture);
  CHECK(manager.find<texture_resource>(test_guid_a) == &texture);
  CHECK(manager.find<image_texture_resource>(test_guid_a) == nullptr);
  CHECK(manager.guid_of(&texture) == test_guid_a);
}
