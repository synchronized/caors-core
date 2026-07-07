// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "object_test_common.h"

using namespace caors::tests::object;

TEST_CASE("object - direct creation supports constructor arguments", "[object]") {
  caors::tracking_resource tracking(&caors::get_system_memory_resource());

  auto* instance = caors::objects::create<direct_non_default_object>(&tracking, 42);

  REQUIRE(instance != nullptr);
  CHECK(instance->value == 42);
  CHECK(instance->type_id() == direct_non_default_object::static_type_id());

  caors::objects::destroy(instance, &tracking);
  CHECK(tracking.statistics().bytes_in_use == 0);
}
