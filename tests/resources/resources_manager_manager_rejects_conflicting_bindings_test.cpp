// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - manager rejects conflicting bindings", "[resources]") {
  caors::resource_manager manager;
  texture_resource first{test_guid_a};
  texture_resource second{test_guid_b};

  REQUIRE(manager.bind(&first));

  CHECK(manager.bind(&first));
  CHECK_FALSE(manager.bind(test_guid_b, &first));
  CHECK_FALSE(manager.bind(test_guid_a, &second));
  CHECK_FALSE(manager.bind(caors::resource_guid::nil(), &second));
  CHECK_FALSE(manager.bind(test_guid_b, nullptr));

  CHECK(manager.size() == 1);
  CHECK(manager.find(test_guid_a) == &first);
}
