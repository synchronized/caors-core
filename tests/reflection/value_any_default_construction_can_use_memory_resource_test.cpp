// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "value_test_common.h"

using namespace caors::tests::reflection;

TEST_CASE("reflection any - default construction can use memory resource", "[reflection]") {
  require_value_test_meta();

  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  caors::any stored = caors::any::default_construct(
      caors::meta::id("caors.test.reflection.any_resource_value"), &tracking);

  REQUIRE(stored.valid());
  REQUIRE(stored.try_get<any_resource_value>() != nullptr);
  CHECK(stored.try_get<any_resource_value>()->value == 17);
  CHECK(tracking.statistics().allocation_count == 1);
  CHECK(tracking.statistics().bytes_in_use >= sizeof(any_resource_value));

  stored.reset();
  CHECK(tracking.statistics().deallocation_count == 1);
  CHECK(tracking.statistics().bytes_in_use == 0);
}
