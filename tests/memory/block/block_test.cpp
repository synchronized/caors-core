// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：memory_block_memory_memory_block_moves_ownership_test.cpp

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

// 原测试文件：memory_block_memory_memory_block_release_transfers_manual_ownership_test.cpp

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

// 原测试文件：memory_block_memory_memory_block_releases_with_selected_resource_test.cpp

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

// 原测试文件：memory_block_memory_memory_block_survives_default_resource_changes_test.cpp

#include "memory_block_test_support.h"

using namespace caors::tests::memory_block;
TEST_CASE("memory - memory block survives default resource changes", "[memory]") {
  caors::tracking_resource first(&caors::get_system_memory_resource());
  caors::tracking_resource second(&caors::get_system_memory_resource());
  caors::memory_resource* old_resource = caors::set_default_memory_resource(&first);

  caors::memory_block block = caors::allocate_block(40, 16);
  caors::memory_resource* replaced_resource = caors::set_default_memory_resource(&second);

  CHECK(replaced_resource == &first);

  block.reset();
  replaced_resource = caors::set_default_memory_resource(old_resource);

  CHECK(replaced_resource == &second);
  CHECK(first.statistics().allocation_count == 1);
  CHECK(first.statistics().deallocation_count == 1);
  CHECK(first.statistics().bytes_in_use == 0);
  CHECK(second.statistics().allocation_count == 0);
  CHECK(second.statistics().deallocation_count == 0);
}

// 原测试文件：memory_block_memory_try_allocate_block_returns_empty_on_failure_test.cpp

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
