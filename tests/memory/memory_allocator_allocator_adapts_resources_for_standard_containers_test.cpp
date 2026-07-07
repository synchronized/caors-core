// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/core/containers.h"
#include "caors/memory.h"

#include <catch2/catch_all.hpp>

#include <string_view>

TEST_CASE("memory - allocator adapts resources for standard containers", "[memory]") {
  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  caors::vector<int> values{caors::allocator<int>(&tracking)};

  values.push_back(1);
  values.push_back(2);
  values.push_back(3);

  CHECK(values[0] == 1);
  CHECK(values[1] == 2);
  CHECK(values[2] == 3);
  CHECK(tracking.statistics().allocation_count > 0);
  CHECK(tracking.statistics().bytes_allocated >= sizeof(int) * values.size());
}
