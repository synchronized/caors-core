// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - manager clears without destroying resources", "[resources]") {
  caors::resource_manager manager;
  texture_resource texture{test_guid_a};

  REQUIRE(manager.bind(&texture));

  manager.clear();

  CHECK(manager.empty());
  CHECK(texture.guid() == test_guid_a);
  CHECK(manager.find(test_guid_a) == nullptr);
}
