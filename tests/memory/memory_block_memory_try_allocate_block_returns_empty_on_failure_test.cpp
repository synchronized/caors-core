// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "memory_block_test_support.h"

using namespace caors::tests::memory_block;
TEST_CASE("memory - try allocate block returns empty on failure", "[memory]") {
  callback_memory_state state;
  caors::memory::callback_memory_resource resource(&test_failed_allocate, &test_deallocate, &state);

  caors::memory_block block = caors::try_allocate_block(48, 32, &resource);

  CHECK_FALSE(block);
  CHECK(block.data() == nullptr);
  CHECK(block.size() == 0);
  CHECK(block.resource() == nullptr);
  CHECK(state.allocate_count == 1);
  CHECK(state.deallocate_count == 0);
}
