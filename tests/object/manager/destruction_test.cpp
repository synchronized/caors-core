// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：object_manager_destruction_manager_clear_releases_live_and_delayed_objects_test.cpp

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

// 原测试文件：object_manager_destruction_manager_destroy_later_invalidates_lookup_before_releasing_object_test.cpp

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

// 原测试文件：object_manager_destruction_manager_rejects_duplicate_delayed_destruction_requests_test.cpp

#include "object_test_common.h"

using namespace caors::tests::object;

TEST_CASE("object - manager rejects duplicate delayed destruction requests", "[object]") {
  caors::objects::manager manager;
  delayed_destroy_actor::destroyed_count = 0;

  const caors::error register_status = register_object_test_type<delayed_destroy_actor>();
  REQUIRE((register_status == caors::error{} ||
           register_status == object_error(caors::errc::duplicate_id)));

  delayed_destroy_actor* instance = manager.create<delayed_destroy_actor>();
  REQUIRE(instance != nullptr);

  const caors::object_id id = instance->id();
  CHECK(manager.destroy_later(instance));
  CHECK_FALSE(manager.destroy_later(instance));
  CHECK_FALSE(manager.destroy_later(id));
  CHECK_FALSE(manager.destroy(instance));
  CHECK(manager.pending_destroy_count() == 1);
  CHECK(delayed_destroy_actor::destroyed_count == 0);

  manager.flush_destroy_queue();

  CHECK(delayed_destroy_actor::destroyed_count == 1);
  CHECK(manager.pending_destroy_count() == 0);
}

// 原测试文件：object_manager_destruction_manager_removes_lookup_entry_before_destruction_test.cpp

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
