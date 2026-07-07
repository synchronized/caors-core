// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "debug_tracking_test_support.h"

using namespace caors::tests::debug_tracking;
TEST_CASE("memory debug tracking - keeps failed allocations out of live records", "[memory]") {
  failing_memory_state state;
  caors::memory::callback_memory_resource failing(&failed_allocate, &failed_deallocate, &state);
  caors::memory::debug_tracking_resource debug(&failing);

  CHECK(debug.try_allocate_debug(48, 32, "failed") == nullptr);
  CHECK(debug.live_allocation_count() == 0);
  CHECK(caors::memory::collect_live_allocations(debug).empty());
  CHECK(debug.debug_statistics().memory.allocation_count == 0);
  CHECK(state.allocate_count == 1);
}
