// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/memory.h"

#include <catch2/catch_all.hpp>

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
