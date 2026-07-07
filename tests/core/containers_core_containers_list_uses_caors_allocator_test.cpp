// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/core/containers.h"
#include "caors/memory/tracking.h"

#include <catch2/catch_all.hpp>

TEST_CASE("core containers - list uses caors allocator", "[core]") {
  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  auto values = caors::make_list<int>(&tracking);

  values.push_back(1);
  values.push_back(2);

  REQUIRE(values.size() == 2);
  CHECK(values.front() == 1);
  CHECK(values.back() == 2);
  CHECK(tracking.statistics().allocation_count > 0);
}
