// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

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
