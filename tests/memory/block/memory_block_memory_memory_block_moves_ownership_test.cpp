// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "memory_block_test_support.h"

using namespace caors::tests::memory_block;
TEST_CASE("memory - memory block moves ownership", "[memory]") {
  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  caors::memory_block block = caors::allocate_block(64, 16, &tracking);
  void* ptr = block.data();

  caors::memory_block moved = std::move(block);

  CHECK_FALSE(block);
  CHECK(moved);
  CHECK(moved.data() == ptr);
  CHECK(moved.resource() == &tracking);

  moved.reset();

  CHECK_FALSE(moved);
  CHECK(tracking.statistics().allocation_count == 1);
  CHECK(tracking.statistics().deallocation_count == 1);
  CHECK(tracking.statistics().bytes_in_use == 0);
}
