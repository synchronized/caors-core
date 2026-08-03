// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/memory.h"

#include <catch2/catch_all.hpp>

#include <array>
#include <bit>
#include <cstdint>
#include <limits>
#include <vector>

TEST_CASE("memory - size class pool routes and reuses pooled blocks", "[memory][size_class]") {
  caors::tracking_resource upstream{&caors::get_system_memory_resource()};
  caors::size_class_pool_options options{};
  options.smallest_block_size = 16;
  options.largest_block_size = 64;
  options.blocks_per_chunk = 2;
  caors::size_class_pool_resource pool{options, &upstream};

  void* first = pool.allocate(17, 8);
  void* second = pool.allocate(31, 8);
  REQUIRE(first != nullptr);
  REQUIRE(second != nullptr);
  CHECK(first != second);

  auto statistics = pool.statistics();
  CHECK(statistics.pooled_allocation_count == 2);
  CHECK(statistics.chunk_count == 1);
  CHECK(statistics.chunk_allocation_count == 1);
  CHECK(statistics.pooled_requested_bytes_in_use == 48);
  CHECK(statistics.pooled_capacity_bytes_in_use == 64);
  CHECK(statistics.reserved_bytes == 64);

  pool.deallocate(first, 17, 8);
  void* reused = pool.allocate(18, 8);
  CHECK(reused == first);

  pool.deallocate(reused, 18, 8);
  pool.deallocate(second, 31, 8);
  CHECK(pool.statistics().pooled_capacity_bytes_in_use == 0);
  pool.release();
  CHECK(upstream.statistics().bytes_in_use == 0);
}

TEST_CASE("memory - size class pool preserves requested alignment", "[memory][size_class]") {
  caors::size_class_pool_options options{};
  options.smallest_block_size = 16;
  options.largest_block_size = 128;
  options.blocks_per_chunk = 4;
  options.pooled_alignment = 64;
  caors::size_class_pool_resource pool{options};

  void* aligned32 = pool.allocate(7, 32);
  void* aligned64 = pool.allocate(1, 64);
  REQUIRE(aligned32 != nullptr);
  REQUIRE(aligned64 != nullptr);
  CHECK(reinterpret_cast<std::uintptr_t>(aligned32) % 32 == 0);
  CHECK(reinterpret_cast<std::uintptr_t>(aligned64) % 64 == 0);

  pool.deallocate(aligned32, 7, 32);
  pool.deallocate(aligned64, 1, 64);
}

TEST_CASE("memory - size class pool falls back for large and over-aligned requests",
          "[memory][size_class]") {
  caors::tracking_resource upstream{&caors::get_system_memory_resource()};
  caors::size_class_pool_options options{};
  options.largest_block_size = 64;
  options.pooled_alignment = 16;
  caors::size_class_pool_resource pool{options, &upstream};

  void* large = pool.allocate(65, 8);
  void* aligned = pool.allocate(16, 64);
  REQUIRE(large != nullptr);
  REQUIRE(aligned != nullptr);
  CHECK(reinterpret_cast<std::uintptr_t>(aligned) % 64 == 0);

  auto statistics = pool.statistics();
  CHECK(statistics.fallback_allocation_count == 2);
  CHECK(statistics.pooled_allocation_count == 0);
  CHECK(statistics.chunk_count == 0);

  pool.deallocate(large, 65, 8);
  pool.deallocate(aligned, 16, 64);
  CHECK(pool.statistics().fallback_deallocation_count == 2);
  CHECK(upstream.statistics().bytes_in_use == 0);
}

TEST_CASE("memory - size class pool resets counters and keeps watermarks", "[memory][size_class]") {
  caors::size_class_pool_options options{};
  options.blocks_per_chunk = 2;
  caors::size_class_pool_resource pool{options};
  void* ptr = pool.allocate(24, 8);
  REQUIRE(ptr != nullptr);

  const auto before = pool.statistics();
  pool.reset_statistics();
  const auto after = pool.statistics();
  CHECK(after.pooled_allocation_count == 0);
  CHECK(after.chunk_count == before.chunk_count);
  CHECK(after.pooled_requested_bytes_in_use == before.pooled_requested_bytes_in_use);
  CHECK(after.pooled_capacity_bytes_in_use == before.pooled_capacity_bytes_in_use);
  CHECK(after.peak_pooled_capacity_bytes_in_use == before.pooled_capacity_bytes_in_use);
  CHECK(after.reserved_bytes == before.reserved_bytes);

  pool.deallocate(ptr, 24, 8);
}

TEST_CASE("memory - size class pool reports upstream allocation failure", "[memory][size_class]") {
  caors::memory::callback_memory_resource failing{
      [](std::size_t, std::size_t, void*) -> void* { return nullptr; },
      [](void*, std::size_t, std::size_t, void*) {}};
  caors::size_class_pool_options options{};
  options.blocks_per_chunk = 2;
  caors::size_class_pool_resource pool{options, &failing};

  CHECK(pool.try_allocate(16, 8) == nullptr);
  CHECK(pool.statistics().allocation_failure_count == 1);
  CHECK(pool.statistics().chunk_count == 0);
  CHECK(pool.try_allocate(8192, 8) == nullptr);
  CHECK(pool.statistics().allocation_failure_count == 2);
}

TEST_CASE("memory - size class pool grows multiple chunks", "[memory][size_class]") {
  caors::size_class_pool_options options{};
  options.blocks_per_chunk = 2;
  caors::size_class_pool_resource pool{options};

  void* first = pool.allocate(24, 8);
  void* second = pool.allocate(24, 8);
  void* third = pool.allocate(24, 8);
  REQUIRE(first != nullptr);
  REQUIRE(second != nullptr);
  REQUIRE(third != nullptr);
  CHECK(pool.statistics().chunk_count == 2);
  CHECK(pool.statistics().chunk_allocation_count == 2);

  pool.deallocate(first, 24, 8);
  pool.deallocate(second, 24, 8);
  pool.deallocate(third, 24, 8);
}

TEST_CASE("memory - size class pool limits large class chunk capacity", "[memory][size_class]") {
  caors::size_class_pool_options options{};
  options.blocks_per_chunk = 64;
  options.target_chunk_size = 4096;
  caors::size_class_pool_resource pool{options};

  std::array<void*, 5> pointers{};
  for (void*& ptr : pointers) {
    ptr = pool.allocate(1024, 8);
    REQUIRE(ptr != nullptr);
  }

  CHECK(pool.statistics().chunk_count == 2);
  CHECK(pool.statistics().chunk_allocation_count == 2);
  CHECK(pool.statistics().reserved_bytes == 8192);

  for (void* ptr : pointers) {
    pool.deallocate(ptr, 1024, 8);
  }
}

TEST_CASE("memory - size class pool capacity follows historical concurrent peak",
          "[memory][size_class]") {
  constexpr std::array<std::size_t, 7> sizes{8, 24, 40, 72, 128, 256, 1024};
  caors::size_class_pool_resource pool;
  const auto run_wave = [&](std::size_t width) {
    std::vector<void*> pointers(width);
    for (std::size_t index = 0; index < width; ++index) {
      pointers[index] = pool.allocate(sizes[index % sizes.size()]);
      REQUIRE(pointers[index] != nullptr);
    }
    for (std::size_t index = width; index != 0; --index) {
      pool.deallocate(pointers[index - 1], sizes[(index - 1) % sizes.size()]);
    }
  };

  run_wave(32);
  const auto low_peak = pool.statistics();
  for (std::size_t wave = 0; wave < 100; ++wave) {
    run_wave(32);
  }
  const auto repeated_low = pool.statistics();
  CHECK(repeated_low.reserved_bytes == low_peak.reserved_bytes);
  CHECK(repeated_low.chunk_count == low_peak.chunk_count);

  run_wave(256);
  const auto high_peak = pool.statistics();
  CHECK(high_peak.reserved_bytes > low_peak.reserved_bytes);
  CHECK(high_peak.chunk_count > low_peak.chunk_count);
  for (std::size_t wave = 0; wave < 100; ++wave) {
    run_wave(128);
  }
  const auto settled = pool.statistics();
  CHECK(settled.reserved_bytes == high_peak.reserved_bytes);
  CHECK(settled.chunk_count == high_peak.chunk_count);
  CHECK(settled.pooled_requested_bytes_in_use == 0);
  CHECK(settled.pooled_capacity_bytes_in_use == 0);

  pool.release();
  CHECK(pool.statistics().reserved_bytes == 0);
  CHECK(pool.statistics().chunk_count == 0);
}

TEST_CASE("memory - debug tracking preserves pool route after mismatched deallocation",
          "[memory][size_class]") {
  struct violation_state {
    std::size_t mismatched_count = 0;
  } violations;

  caors::size_class_pool_resource pool;
  caors::memory::debug_tracking_resource debug{&pool};
  debug.set_violation_callback(
      [](const caors::memory::memory_debug_violation& violation, void* user_data) noexcept {
        if (violation.kind == caors::memory::memory_debug_violation_kind::mismatched_deallocation) {
          ++static_cast<violation_state*>(user_data)->mismatched_count;
        }
      },
      &violations);

  void* ptr = debug.allocate(24, 8);
  debug.deallocate(ptr, 48, 8);
  CHECK(violations.mismatched_count == 1);
  CHECK(debug.debug_statistics().mismatched_deallocation_count == 1);
  CHECK(pool.statistics().pooled_capacity_bytes_in_use == 0);
}

TEST_CASE("memory - size class pool exposes configured options", "[memory][size_class]") {
  caors::size_class_pool_options options{};
  options.smallest_block_size = 32;
  options.largest_block_size = 256;
  options.blocks_per_chunk = 7;
  options.target_chunk_size = 2048;
  options.pooled_alignment = 32;
  caors::size_class_pool_resource pool{options};

  CHECK(pool.options().smallest_block_size == 32);
  CHECK(pool.options().largest_block_size == 256);
  CHECK(pool.options().blocks_per_chunk == 7);
  CHECK(pool.options().target_chunk_size == 2048);
  CHECK(pool.options().pooled_alignment == 32);
  CHECK(pool.upstream_resource() == &caors::get_system_memory_resource());
}

TEST_CASE("memory - size class pool normalizes zero-sized allocation", "[memory][size_class]") {
  caors::size_class_pool_options options{};
  options.blocks_per_chunk = 2;
  caors::size_class_pool_resource pool{options};

  void* ptr = pool.allocate(0, 8);
  REQUIRE(ptr != nullptr);
  CHECK(pool.statistics().pooled_requested_bytes_in_use == 1);
  pool.deallocate(ptr, 0, 8);
  CHECK(pool.statistics().pooled_requested_bytes_in_use == 0);
}

TEST_CASE("memory - size class pool handles maximum class without arithmetic overflow",
          "[memory][size_class]") {
  const std::size_t largest_power = std::bit_floor(std::numeric_limits<std::size_t>::max());
  caors::size_class_pool_options options{};
  options.smallest_block_size = largest_power;
  options.largest_block_size = largest_power;
  options.blocks_per_chunk = 2;
  caors::size_class_pool_resource pool{options};

  CHECK(pool.try_allocate(largest_power, 8) == nullptr);
  CHECK(pool.statistics().allocation_failure_count == 1);
  CHECK(pool.statistics().chunk_count == 0);
}
