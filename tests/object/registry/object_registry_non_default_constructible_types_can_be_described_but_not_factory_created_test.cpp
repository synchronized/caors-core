// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "object_test_common.h"

using namespace caors::tests::object;

TEST_CASE("object - non default constructible types can be described but not factory-created",
          "[object]") {
  caors::objects::registry& registry = caors::objects::get_registry();

  REQUIRE(register_object_test_type<registered_non_default_object>() == caors::error{});

  auto type = caors::objects::find<registered_non_default_object>();
  REQUIRE(type);
  CHECK_FALSE(type.constructible());
  CHECK(registry.try_create<registered_non_default_object>() == nullptr);
  CHECK(registry.create<registered_non_default_object>() == nullptr);
}
