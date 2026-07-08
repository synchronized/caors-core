// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：object_ref_locked_weak_refs_keep_intrusive_ref_counted_objects_alive_test.cpp

#include "object_test_common.h"

using namespace caors::tests::object;

TEST_CASE("object - locked weak refs keep intrusive ref counted objects alive", "[object]") {
  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  counted_actor::destroyed_count = 0;

  caors::objects::weak_ref<counted_actor> weak;
  caors::objects::ref<counted_actor> locked;

  {
    caors::objects::ref<counted_actor> owner =
        caors::objects::make_ref<counted_actor>(&tracking, 9);
    weak = owner;
    locked = weak.lock();

    REQUIRE(locked);
    CHECK(owner.use_count() == 2);

    owner.reset();
    CHECK(counted_actor::destroyed_count == 0);
    CHECK(weak.use_count() == 1);
    CHECK_FALSE(weak.expired());
  }

  CHECK(counted_actor::destroyed_count == 0);
  CHECK(locked->value == 9);

  locked.reset();

  CHECK(counted_actor::destroyed_count == 1);
  CHECK(weak.expired());

  weak.reset();

  const caors::memory_statistics statistics = tracking.statistics();
  CHECK(statistics.bytes_in_use == 0);
  CHECK(statistics.allocation_count == statistics.deallocation_count);
}

// 原测试文件：object_ref_ptr_handle_and_ref_keep_separate_lifetime_semantics_test.cpp

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

// 原测试文件：object_ref_ref_and_weak_ref_support_safe_upcasts_test.cpp

#include "object_test_common.h"

using namespace caors::tests::object;

TEST_CASE("object - ref and weak_ref support safe upcasts", "[object]") {
  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  counted_actor::destroyed_count = 0;
  counted_player::destroyed_count = 0;

  caors::objects::weak_ref<counted_actor> weak_base;

  {
    caors::objects::ref<counted_player> player =
        caors::objects::make_ref<counted_player>(&tracking);
    player->value = 33;

    caors::objects::ref<counted_actor> copied = player;
    caors::objects::ref<counted_actor> assigned;
    assigned = player;

    CHECK(copied.get() == static_cast<counted_actor*>(player.get()));
    CHECK(assigned.get() == static_cast<counted_actor*>(player.get()));
    CHECK(player.use_count() == 3);

    caors::objects::weak_ref<counted_player> weak_player = player;
    caors::objects::weak_ref<counted_actor> weak_from_strong = player;
    caors::objects::weak_ref<counted_actor> weak_from_weak = weak_player;
    weak_base = weak_player;

    CHECK_FALSE(weak_base.expired());
    CHECK(weak_from_strong.use_count() == 3);
    CHECK(weak_from_weak.use_count() == 3);

    caors::objects::ref<counted_actor> locked = weak_base.lock();
    REQUIRE(locked);
    CHECK(locked.get() == static_cast<counted_actor*>(player.get()));
    CHECK(locked->value == 33);
    CHECK(player.use_count() == 4);

    locked.reset();
    CHECK(player.use_count() == 3);

    caors::objects::weak_ref<counted_actor> moved_weak = std::move(weak_from_weak);
    CHECK(weak_from_weak == nullptr);
    CHECK_FALSE(moved_weak.expired());

    caors::objects::ref<counted_actor> moved = std::move(player);
    CHECK(player == nullptr);
    REQUIRE(moved);
    CHECK(moved->value == 33);
    CHECK(moved.use_count() == 3);

    copied.reset();
    assigned.reset();
    CHECK(moved.use_count() == 1);
  }

  CHECK(counted_player::destroyed_count == 1);
  CHECK(counted_actor::destroyed_count == 1);
  CHECK(weak_base.expired());
  CHECK(tracking.statistics().bytes_in_use > 0);

  weak_base.reset();

  const caors::memory_statistics statistics = tracking.statistics();
  CHECK(statistics.bytes_in_use == 0);
  CHECK(statistics.allocation_count == statistics.deallocation_count);
}

// 原测试文件：object_ref_strong_refs_own_intrusive_ref_counted_objects_test.cpp

#include "object_test_common.h"

using namespace caors::tests::object;

TEST_CASE("object - strong refs own intrusive ref counted objects", "[object]") {
  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  counted_actor::destroyed_count = 0;

  {
    caors::objects::ref<counted_actor> owner =
        caors::objects::make_ref<counted_actor>(&tracking, 42);

    REQUIRE(owner);
    CHECK(owner->value == 42);
    CHECK(owner.use_count() == 1);
    CHECK(owner.unique());

    {
      caors::objects::ref<counted_actor> copy = owner;
      CHECK(copy.get() == owner.get());
      CHECK(owner.use_count() == 2);
      CHECK_FALSE(owner.unique());

      caors::objects::ref<counted_actor> moved = std::move(copy);
      CHECK(copy == nullptr);
      CHECK(moved.get() == owner.get());
      CHECK(owner.use_count() == 2);
    }

    CHECK(owner.use_count() == 1);
    CHECK(counted_actor::destroyed_count == 0);

    owner.reset();
    CHECK(owner == nullptr);
    CHECK(counted_actor::destroyed_count == 1);
  }

  const caors::memory_statistics statistics = tracking.statistics();
  CHECK(statistics.bytes_in_use == 0);
  CHECK(statistics.allocation_count == statistics.deallocation_count);
}

// 原测试文件：object_ref_weak_refs_observe_intrusive_ref_counted_object_lifetime_test.cpp

#include "object_test_common.h"

using namespace caors::tests::object;

TEST_CASE("object - weak refs observe intrusive ref counted object lifetime", "[object]") {
  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  counted_actor::destroyed_count = 0;

  caors::objects::weak_ref<counted_actor> weak;

  {
    caors::objects::ref<counted_actor> owner =
        caors::objects::make_ref<counted_actor>(&tracking, 7);
    weak = owner;

    REQUIRE(weak);
    CHECK_FALSE(weak.expired());
    CHECK(weak.use_count() == 1);

    {
      caors::objects::ref<counted_actor> locked = weak.lock();
      REQUIRE(locked);
      CHECK(locked.get() == owner.get());
      CHECK(locked->value == 7);
      CHECK(owner.use_count() == 2);
      CHECK(weak.use_count() == 2);
    }

    CHECK(owner.use_count() == 1);
    CHECK(weak.use_count() == 1);
    CHECK(counted_actor::destroyed_count == 0);
  }

  CHECK(counted_actor::destroyed_count == 1);
  CHECK(weak.expired());
  CHECK(weak == nullptr);
  CHECK(weak.use_count() == 0);
  CHECK(weak.lock() == nullptr);
  CHECK(tracking.statistics().bytes_in_use > 0);

  weak.reset();

  const caors::memory_statistics statistics = tracking.statistics();
  CHECK(statistics.bytes_in_use == 0);
  CHECK(statistics.allocation_count == statistics.deallocation_count);
}
