// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "object_test_common.h"

using namespace caors::tests::object;

TEST_CASE("object - ptr handle and ref keep separate lifetime semantics", "[object]") {
  caors::tracking_resource tracking(&caors::get_system_memory_resource());

  direct_non_default_object* raw = caors::objects::create<direct_non_default_object>(&tracking, 11);
  REQUIRE(raw != nullptr);

  {
    caors::objects::ptr<direct_non_default_object> ptr{raw};
    CHECK(ptr);
    CHECK(ptr.get() == raw);
    CHECK(ptr.id() == raw->id());
  }

  CHECK(tracking.statistics().bytes_in_use > 0);
  caors::objects::destroy(raw, &tracking);
  CHECK(tracking.statistics().bytes_in_use == 0);

  const caors::error actor_status = register_object_test_type<managed_actor>();
  REQUIRE(
      (actor_status == caors::error{} || actor_status == object_error(caors::errc::duplicate_id)));

  {
    caors::objects::manager manager(&tracking);
    managed_actor* managed = manager.create<managed_actor>();
    REQUIRE(managed != nullptr);

    const caors::objects::handle handle = manager.handle_of(managed->id());
    const caors::objects::typed_handle<managed_actor> typed =
        manager.handle_of<managed_actor>(managed->id());

    REQUIRE(handle);
    REQUIRE(typed);
    CHECK(manager.find(handle) == managed);
    CHECK(manager.find(typed) == managed);

    CHECK(manager.destroy(managed));
    CHECK_FALSE(manager.alive(handle));
    CHECK(manager.find(typed) == nullptr);
  }

  CHECK(tracking.statistics().bytes_in_use == 0);

  counted_actor::destroyed_count = 0;
  {
    caors::objects::ref<counted_actor> owner =
        caors::objects::make_ref<counted_actor>(&tracking, 17);
    caors::objects::weak_ref<counted_actor> weak = owner;

    REQUIRE(owner);
    CHECK(weak.lock().get() == owner.get());
    CHECK(owner.use_count() == 1);
  }

  CHECK(counted_actor::destroyed_count == 1);
  CHECK(tracking.statistics().bytes_in_use == 0);
}
