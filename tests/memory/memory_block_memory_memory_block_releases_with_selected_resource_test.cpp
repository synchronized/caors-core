// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "memory_block_test_support.h"

using namespace caors::tests::memory_block;
TEST_CASE("memory - memory block releases with selected resource", "[memory]") {
  caors::tracking_resource tracking(&caors::get_system_memory_resource());

  {
    caors::memory_block block = caors::allocate_block(96, 32, &tracking);

    REQUIRE(block);
    REQUIRE(block.data() != nullptr);
    CHECK(block.size() == 96);
    CHECK(block.alignment() == 32);
    CHECK(block.resource() == &tracking);
    CHECK(reinterpret_cast<std::uintptr_t>(block.data()) % 32 == 0);
    CHECK(tracking.statistics().allocation_count == 1);
    CHECK(tracking.statistics().bytes_in_use == 96);
  }

  CHECK(tracking.statistics().deallocation_count == 1);
  CHECK(tracking.statistics().bytes_in_use == 0);
}
