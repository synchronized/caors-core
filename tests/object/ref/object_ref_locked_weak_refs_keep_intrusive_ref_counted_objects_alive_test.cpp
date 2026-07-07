// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

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
