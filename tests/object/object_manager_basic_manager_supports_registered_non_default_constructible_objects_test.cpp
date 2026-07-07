// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "object_test_common.h"

using namespace caors::tests::object;

TEST_CASE("object - manager supports registered non default constructible objects", "[object]") {
  caors::objects::manager manager;

  REQUIRE(register_object_test_type<managed_non_default_object>() == caors::error{});

  CHECK(manager.create(managed_non_default_object::static_type_id()) == nullptr);

  managed_non_default_object* instance = manager.create<managed_non_default_object>(42);
  REQUIRE(instance != nullptr);
  CHECK(instance->value == 42);
  CHECK(manager.find<managed_non_default_object>(instance->id()) == instance);

  CHECK(manager.destroy(instance));
  CHECK(manager.empty());
}
