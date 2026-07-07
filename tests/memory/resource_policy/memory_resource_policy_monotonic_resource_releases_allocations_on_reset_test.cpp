// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/memory.h"

#include <catch2/catch_all.hpp>

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
