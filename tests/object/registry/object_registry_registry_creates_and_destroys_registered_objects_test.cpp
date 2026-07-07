// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "object_test_common.h"

using namespace caors::tests::object;

TEST_CASE("object - registry creates and destroys registered objects", "[object]") {
  caors::objects::registry& registry = caors::objects::get_registry();
  caors::tracking_resource tracking(&caors::get_system_memory_resource());

  REQUIRE(register_object_test_type<factory_actor>() == caors::error{});

  const auto actor_type = caors::objects::find<factory_actor>();
  REQUIRE(actor_type);

  caors::object* instance = registry.create(actor_type.id(), &tracking);

  REQUIRE(instance != nullptr);
  CHECK(instance->type_id() == actor_type.id());
  CHECK(tracking.statistics().allocation_count == 1);

  CHECK(registry.destroy(instance, &tracking));
  CHECK(tracking.statistics().deallocation_count == 1);
  CHECK(tracking.statistics().bytes_in_use == 0);
}
