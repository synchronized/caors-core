// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "memory_block_test_support.h"

using namespace caors::tests::memory_block;
TEST_CASE("memory - memory block release transfers manual ownership", "[memory]") {
  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  caors::memory_block block = caors::allocate_block(80, 16, &tracking);
  caors::memory_resource* resource = block.resource();
  const std::size_t size = block.size();
  const std::size_t alignment = block.alignment();

  void* ptr = block.release();

  REQUIRE(ptr != nullptr);
  CHECK_FALSE(block);
  CHECK(block.size() == size);
  CHECK(block.alignment() == alignment);
  CHECK(block.resource() == resource);
  CHECK(tracking.statistics().deallocation_count == 0);

  resource->deallocate(ptr, size, alignment);

  CHECK(tracking.statistics().deallocation_count == 1);
  CHECK(tracking.statistics().bytes_in_use == 0);
}
