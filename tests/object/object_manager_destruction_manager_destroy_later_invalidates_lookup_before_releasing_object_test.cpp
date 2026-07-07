// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "object_test_common.h"

using namespace caors::tests::object;

TEST_CASE("object - manager destroy_later invalidates lookup before releasing object", "[object]") {
  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  delayed_destroy_actor::destroyed_count = 0;

  {
    caors::objects::object_index index(&tracking);
    caors::objects::manager manager(index, &tracking);

    const caors::error register_status = register_object_test_type<delayed_destroy_actor>();
    REQUIRE((register_status == caors::error{} ||
             register_status == object_error(caors::errc::duplicate_id)));

    delayed_destroy_actor* instance =
        manager.create_with_guid<delayed_destroy_actor>(test_object_guid_a);
    REQUIRE(instance != nullptr);
    instance->value = 24;

    const caors::object_id id = instance->id();
    const caors::objects::handle handle = manager.handle_of(id);
    const caors::objects::ptr<delayed_destroy_actor> ptr{instance};

    REQUIRE(handle);
    REQUIRE(ptr);
    REQUIRE(index.find(test_object_guid_a) == id);

    CHECK(manager.destroy_later(id));
    CHECK(manager.empty());
    CHECK(manager.pending_destroy_count() == 1);
    CHECK_FALSE(manager.contains(id));
    CHECK_FALSE(manager.alive(handle));
    CHECK(manager.find(id) == nullptr);
    CHECK(manager.find(handle) == nullptr);
    CHECK(index.empty());
    CHECK(index.find(test_object_guid_a) == caors::object_id::invalid());
    CHECK(delayed_destroy_actor::destroyed_count == 0);
    CHECK(ptr.id() == id);
    CHECK(instance->value == 24);

    manager.flush_destroy_queue();

    CHECK(manager.pending_destroy_count() == 0);
    CHECK(delayed_destroy_actor::destroyed_count == 1);
    CHECK_FALSE(manager.destroy_later(id));
  }

  const caors::memory_statistics statistics = tracking.statistics();
  CHECK(statistics.bytes_in_use == 0);
  CHECK(statistics.allocation_count == statistics.deallocation_count);
}
