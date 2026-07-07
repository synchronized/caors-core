// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "object_test_common.h"

using namespace caors::tests::object;

TEST_CASE("object - manager owns objects and resolves handles", "[object]") {
  caors::tracking_resource tracking(&caors::get_system_memory_resource());

  {
    caors::objects::manager manager(&tracking);

    const caors::error actor_status = register_object_test_type<managed_actor>();
    REQUIRE((actor_status == caors::error{} ||
             actor_status == object_error(caors::errc::duplicate_id)));

    managed_actor* instance = manager.create<managed_actor>();
    REQUIRE(instance != nullptr);
    instance->value = 13;

    const caors::object_id id = instance->id();
    const caors::objects::handle handle = manager.handle_of(id);

    CHECK(manager.size() == 1);
    CHECK(manager.contains(id));
    CHECK(handle);
    CHECK(handle.id() == id);
    CHECK(handle.type() == managed_actor::static_type_id());
    CHECK(manager.find(id) == instance);
    CHECK(manager.find(handle) == instance);
    CHECK(manager.find<managed_actor>(id) == instance);
    CHECK(manager.find<managed_actor>(handle) == instance);
    CHECK(manager.alive(handle));

    CHECK(manager.destroy(id));
    CHECK(manager.empty());
    CHECK_FALSE(manager.contains(id));
    CHECK_FALSE(manager.alive(handle));
    CHECK(manager.find(id) == nullptr);
    CHECK(manager.find(handle) == nullptr);
    CHECK_FALSE(manager.destroy(id));
  }

  CHECK(tracking.statistics().bytes_in_use == 0);
}
