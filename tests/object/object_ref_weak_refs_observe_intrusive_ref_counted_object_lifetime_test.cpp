// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

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
