// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - manager unbinds by guid and resource pointer", "[resources]") {
  caors::resource_manager manager;
  texture_resource first{test_guid_a};
  texture_resource second{test_guid_b};

  REQUIRE(manager.bind(&first));
  REQUIRE(manager.bind(&second));

  CHECK(manager.unbind(test_guid_a));
  CHECK_FALSE(manager.unbind(test_guid_a));
  CHECK_FALSE(manager.contains(test_guid_a));
  CHECK_FALSE(manager.contains(&first));
  CHECK(manager.find(test_guid_a) == nullptr);
  CHECK(manager.guid_of(&first) == caors::resource_guid::nil());

  CHECK(manager.unbind(&second));
  CHECK(manager.empty());
  CHECK(manager.find(test_guid_b) == nullptr);
}
