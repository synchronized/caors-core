// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

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
