// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "object_test_common.h"

using namespace caors::tests::object;

TEST_CASE("object - manager removes lookup entry before destruction", "[object]") {
  caors::objects::manager manager;

  const caors::error register_status = register_object_test_type<destruction_observer>();
  REQUIRE((register_status == caors::error{} ||
           register_status == object_error(caors::errc::duplicate_id)));

  destruction_observer::manager = &manager;
  destruction_observer::invisible_during_destruction = false;

  destruction_observer* instance = manager.create<destruction_observer>();
  REQUIRE(instance != nullptr);

  const caors::object_id id = instance->id();
  REQUIRE(manager.find(id) == instance);

  CHECK(manager.destroy(id));
  CHECK(destruction_observer::invisible_during_destruction);
  CHECK(manager.find(id) == nullptr);

  destruction_observer::manager = nullptr;
}
