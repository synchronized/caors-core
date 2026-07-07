// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - ref can hold unresolved guid", "[resources]") {
  caors::resources::ref<texture_resource> reference{test_guid_a};

  CHECK(reference);
  CHECK(reference.has_guid());
  CHECK_FALSE(reference.resolved());
  CHECK(reference.get() == nullptr);
  CHECK(reference.guid() == test_guid_a);

  reference.unresolve();
  CHECK(reference.guid() == test_guid_a);
}
