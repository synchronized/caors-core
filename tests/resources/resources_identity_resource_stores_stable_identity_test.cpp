// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - resource stores stable identity", "[resources]") {
  texture_resource texture;

  CHECK_FALSE(texture.has_guid());
  CHECK(texture.guid() == caors::resource_guid::nil());

  texture.set_guid(test_guid_a);

  CHECK(texture.has_guid());
  CHECK(texture.guid() == test_guid_a);
}
