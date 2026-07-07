// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "debug_tracking_test_support.h"

using namespace caors::tests::debug_tracking;
TEST_CASE("memory debug tracking - detects deallocation mismatches", "[memory]") {
  caors::memory::debug_tracking_resource debug(&caors::get_system_memory_resource());

  void* ptr = debug.allocate(32, 16);
  debug.deallocate(ptr, 64, 16);

  const auto stats = debug.debug_statistics();
  CHECK(stats.memory.deallocation_count == 1);
  CHECK(stats.mismatched_deallocation_count == 1);
  CHECK(stats.live_allocation_count == 0);
  CHECK(stats.memory.bytes_in_use == 0);
}
