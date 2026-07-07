// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "object_test_common.h"

using namespace caors::tests::object;

TEST_CASE("object - manager clear releases live and delayed objects", "[object]") {
  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  delayed_destroy_actor::destroyed_count = 0;

  {
    caors::objects::object_index index(&tracking);
    caors::objects::manager manager(index, &tracking);

    const caors::error register_status = register_object_test_type<delayed_destroy_actor>();
    REQUIRE((register_status == caors::error{} ||
             register_status == object_error(caors::errc::duplicate_id)));

    delayed_destroy_actor* delayed =
        manager.create_with_guid<delayed_destroy_actor>(test_object_guid_a);
    delayed_destroy_actor* live =
        manager.create_with_guid<delayed_destroy_actor>(test_object_guid_b);
    REQUIRE(delayed != nullptr);
    REQUIRE(live != nullptr);
    REQUIRE(manager.size() == 2);
    REQUIRE(index.size() == 2);

    CHECK(manager.destroy_later(delayed));
    CHECK(manager.size() == 1);
    CHECK(manager.pending_destroy_count() == 1);
    CHECK(index.find(test_object_guid_a) == caors::object_id::invalid());
    CHECK(index.find(test_object_guid_b) == live->id());
    CHECK(delayed_destroy_actor::destroyed_count == 0);

    manager.clear();

    CHECK(manager.empty());
    CHECK(manager.pending_destroy_count() == 0);
    CHECK(index.empty());
    CHECK(delayed_destroy_actor::destroyed_count == 2);
  }

  const caors::memory_statistics statistics = tracking.statistics();
  CHECK(statistics.bytes_in_use == 0);
  CHECK(statistics.allocation_count == statistics.deallocation_count);
}
