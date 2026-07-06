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

TEST_CASE("memory - monotonic resource releases allocations on reset", "[memory]") {
  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  caors::monotonic_resource monotonic(64, &tracking);

  void* first = monotonic.allocate(16, 16);
  void* second = monotonic.allocate(16, 16);

  REQUIRE(first != nullptr);
  REQUIRE(second != nullptr);
  CHECK(first != second);
  CHECK(tracking.statistics().allocation_count > 0);
  CHECK(tracking.statistics().deallocation_count == 0);

  monotonic.deallocate(first, 16, 16);
  CHECK(tracking.statistics().deallocation_count == 0);

  monotonic.reset();

  CHECK(tracking.statistics().bytes_in_use == 0);
  CHECK(tracking.statistics().deallocation_count == tracking.statistics().allocation_count);
}

TEST_CASE("memory - fixed block pool reuses released blocks", "[memory]") {
  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  caors::fixed_block_pool pool(sizeof(int), alignof(int), 2, &tracking);

  void* first = pool.allocate(sizeof(int), alignof(int));
  void* second = pool.allocate(sizeof(int), alignof(int));

  REQUIRE(first != nullptr);
  REQUIRE(second != nullptr);
  CHECK(first != second);

  pool.deallocate(first, sizeof(int), alignof(int));
  void* reused = pool.allocate(sizeof(int), alignof(int));

  CHECK(reused == first);

  pool.deallocate(reused, sizeof(int), alignof(int));
  pool.deallocate(second, sizeof(int), alignof(int));
  pool.release();

  CHECK(tracking.statistics().bytes_in_use == 0);
}
