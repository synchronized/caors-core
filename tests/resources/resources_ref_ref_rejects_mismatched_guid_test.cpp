// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - ref rejects mismatched guid", "[resources]") {
  texture_resource texture{test_guid_b};
  caors::resources::ref<texture_resource> reference{test_guid_a};

  CHECK_FALSE(reference.resolve(&texture));
  CHECK_FALSE(reference.resolved());
  CHECK(reference.guid() == test_guid_a);
}
