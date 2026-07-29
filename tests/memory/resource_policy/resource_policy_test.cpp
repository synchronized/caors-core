// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：memory_resource_policy_fixed_block_pool_reuses_released_blocks_test.cpp

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

// 原测试文件：memory_resource_policy_monotonic_resource_releases_allocations_on_reset_test.cpp

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

TEST_CASE("memory - monotonic resource reports allocation statistics", "[memory]") {
  caors::monotonic_resource monotonic(64);

  REQUIRE(monotonic.allocate(16, 16) != nullptr);
  const auto ordinary_statistics = monotonic.statistics();
  CHECK(ordinary_statistics.bytes_in_use >= 16);
  CHECK(ordinary_statistics.peak_bytes_in_use == ordinary_statistics.bytes_in_use);
  CHECK(ordinary_statistics.chunk_count == 1);
  CHECK(ordinary_statistics.peak_chunk_count == 1);
  CHECK(ordinary_statistics.large_allocation_count == 0);

  REQUIRE(monotonic.allocate(4096, 16) != nullptr);
  const auto large_statistics = monotonic.statistics();
  CHECK(large_statistics.bytes_in_use > ordinary_statistics.bytes_in_use);
  CHECK(large_statistics.peak_bytes_in_use == large_statistics.bytes_in_use);
  CHECK(large_statistics.chunk_count == 2);
  CHECK(large_statistics.peak_chunk_count == 2);
  CHECK(large_statistics.large_allocation_count == 1);

  monotonic.reset();
  const auto reset_statistics = monotonic.statistics();
  CHECK(reset_statistics.bytes_in_use == 0);
  CHECK(reset_statistics.peak_bytes_in_use == large_statistics.peak_bytes_in_use);
  CHECK(reset_statistics.chunk_count == 0);
  CHECK(reset_statistics.peak_chunk_count == 2);
  CHECK(reset_statistics.reset_count == 1);
}

TEST_CASE("memory - monotonic resource resets counters without changing current watermarks",
          "[memory]") {
  caors::monotonic_resource monotonic(64);
  REQUIRE(monotonic.allocate(24, 8) != nullptr);

  const auto before_reset = monotonic.statistics();
  monotonic.reset_statistics();
  const auto after_reset = monotonic.statistics();

  CHECK(after_reset.bytes_in_use == before_reset.bytes_in_use);
  CHECK(after_reset.peak_bytes_in_use == before_reset.bytes_in_use);
  CHECK(after_reset.chunk_count == before_reset.chunk_count);
  CHECK(after_reset.peak_chunk_count == before_reset.chunk_count);
  CHECK(after_reset.large_allocation_count == 0);
  CHECK(after_reset.reset_count == 0);
}

// 原测试文件：memory_resource_policy_tracking_resource_records_allocation_statistics_test.cpp

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
