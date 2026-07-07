// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/core/containers.h"
#include "caors/memory.h"

#include <catch2/catch_all.hpp>

#include <string_view>

TEST_CASE("memory - make and destroy use the selected resource", "[memory]") {
  struct object {
    explicit object(int value) : value(value) {}
    int value = 0;
  };

  caors::tracking_resource tracking(&caors::get_system_memory_resource());

  object* ptr = caors::make<object>(&tracking, 42);

  REQUIRE(ptr != nullptr);
  CHECK(ptr->value == 42);
  CHECK(tracking.statistics().allocation_count == 1);

  caors::destroy(ptr, &tracking);

  CHECK(tracking.statistics().deallocation_count == 1);
  CHECK(tracking.statistics().bytes_in_use == 0);
}
