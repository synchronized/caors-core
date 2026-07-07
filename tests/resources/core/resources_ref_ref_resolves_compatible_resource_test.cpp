// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - ref resolves compatible resource", "[resources]") {
  texture_resource texture{test_guid_a};
  caors::resources::ref<texture_resource> reference{test_guid_a};

  REQUIRE(reference.resolve(&texture));

  CHECK(reference.resolved());
  CHECK(reference.get() == &texture);
  CHECK(reference->guid() == test_guid_a);
  CHECK((*reference).guid() == test_guid_a);

  reference.unresolve();

  CHECK_FALSE(reference.resolved());
  CHECK(reference.guid() == test_guid_a);
}
