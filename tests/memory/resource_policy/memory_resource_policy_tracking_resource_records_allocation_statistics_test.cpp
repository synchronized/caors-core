// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/memory.h"

#include <catch2/catch_all.hpp>

TEST_CASE("memory - tracking resource records allocation statistics", "[memory]") {
  caors::tracking_resource tracking(&caors::get_system_memory_resource());

  void* ptr = tracking.allocate(80, 16);

  auto stats = tracking.statistics();
  CHECK(stats.allocation_count == 1);
  CHECK(stats.bytes_allocated == 80);
  CHECK(stats.bytes_in_use == 80);
  CHECK(stats.peak_bytes_in_use == 80);

  tracking.deallocate(ptr, 80, 16);

  stats = tracking.statistics();
  CHECK(stats.deallocation_count == 1);
  CHECK(stats.bytes_deallocated == 80);
  CHECK(stats.bytes_in_use == 0);
}
