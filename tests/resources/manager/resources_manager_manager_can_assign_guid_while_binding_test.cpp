// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - manager can assign guid while binding", "[resources]") {
  caors::resource_manager manager;
  texture_resource texture;

  REQUIRE(manager.bind(test_guid_a, &texture));

  CHECK(texture.guid() == test_guid_a);
  CHECK(manager.find(test_guid_a) == &texture);
}
