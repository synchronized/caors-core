// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "support/benchmark_support.h"

#include "caors/memory/fixed_block_pool.h"
#include "caors/memory/size_class_pool.h"

#include <ctrack.hpp>

#include <array>
#include <cstddef>
#include <iostream>
#include <memory_resource>

namespace caors::benchmarks {
namespace {

constexpr std::array<std::size_t, 8> mixed_sizes{8, 24, 40, 72, 128, 256, 1024, 5000};
constexpr std::size_t pooled_requested_bytes = 1552;

void run_caors_batch(caors::memory_resource& resource, std::size_t iterations) {
  std::array<void*, mixed_sizes.size()> pointers{};
  for (std::size_t iteration = 0; iteration < iterations; ++iteration) {
    for (std::size_t index = 0; index < mixed_sizes.size(); ++index) {
      pointers[index] = resource.allocate(mixed_sizes[index]);
      consume_address(pointers[index]);
    }
    for (std::size_t index = mixed_sizes.size(); index != 0; --index) {
      resource.deallocate(pointers[index - 1], mixed_sizes[index - 1]);
    }
  }
}

void run_pmr_batch(std::pmr::memory_resource& resource, std::size_t iterations) {
  std::array<void*, mixed_sizes.size()> pointers{};
  for (std::size_t iteration = 0; iteration < iterations; ++iteration) {
    for (std::size_t index = 0; index < mixed_sizes.size(); ++index) {
      pointers[index] = resource.allocate(mixed_sizes[index]);
      consume_address(pointers[index]);
    }
    for (std::size_t index = mixed_sizes.size(); index != 0; --index) {
      resource.deallocate(pointers[index - 1], mixed_sizes[index - 1]);
    }
  }
}

class manual_fixed_pools {
public:
  explicit manual_fixed_pools(caors::memory_resource* upstream)
      : pool16_(16, caors::default_memory_alignment, 64, upstream),
        pool32_(32, caors::default_memory_alignment, 64, upstream),
        pool64_(64, caors::default_memory_alignment, 64, upstream),
        pool128_(128, caors::default_memory_alignment, 64, upstream),
        pool256_(256, caors::default_memory_alignment, 64, upstream),
        pool1024_(1024, caors::default_memory_alignment, 64, upstream), upstream_(upstream) {}

  void run(std::size_t iterations) {
    std::array<void*, mixed_sizes.size()> pointers{};
    for (std::size_t iteration = 0; iteration < iterations; ++iteration) {
      for (std::size_t index = 0; index < mixed_sizes.size(); ++index) {
        pointers[index] = allocate(mixed_sizes[index]);
        consume_address(pointers[index]);
      }
      for (std::size_t index = mixed_sizes.size(); index != 0; --index) {
        deallocate(pointers[index - 1], mixed_sizes[index - 1]);
      }
    }
  }

private:
  [[nodiscard]] caors::memory_resource& select(std::size_t size) noexcept {
    if (size <= 16) {
      return pool16_;
    }
    if (size <= 32) {
      return pool32_;
    }
    if (size <= 64) {
      return pool64_;
    }
    if (size <= 128) {
      return pool128_;
    }
    if (size <= 256) {
      return pool256_;
    }
    if (size <= 1024) {
      return pool1024_;
    }
    return *upstream_;
  }

  [[nodiscard]] void* allocate(std::size_t size) { return select(size).allocate(size); }

  void deallocate(void* ptr, std::size_t size) noexcept { select(size).deallocate(ptr, size); }

  caors::fixed_block_pool pool16_;
  caors::fixed_block_pool pool32_;
  caors::fixed_block_pool pool64_;
  caors::fixed_block_pool pool128_;
  caors::fixed_block_pool pool256_;
  caors::fixed_block_pool pool1024_;
  caors::memory_resource* upstream_ = nullptr;
};

} // namespace

void benchmark_memory_pools(std::size_t iterations) {
  caors::memory_resource& system = caors::get_system_memory_resource();

  {
    CTRACK_NAME("memory.pool.system_mixed_batch");
    run_caors_batch(system, iterations);
  }

  {
    manual_fixed_pools pools{&system};
    pools.run(1);
    CTRACK_NAME("memory.pool.manual_fixed_mixed_batch");
    pools.run(iterations);
  }

  caors::size_class_pool_options options{};
  options.blocks_per_chunk = 64;
  caors::size_class_pool_resource pool{options, &system};
  run_caors_batch(pool, 1);
  pool.reset_statistics();
  {
    CTRACK_NAME("memory.pool.size_class_mixed_batch");
    run_caors_batch(pool, iterations);
  }

  {
    std::pmr::pool_options pmr_options{};
    pmr_options.max_blocks_per_chunk = 64;
    pmr_options.largest_required_pool_block = 4096;
    std::pmr::unsynchronized_pool_resource pmr_pool{pmr_options};
    run_pmr_batch(pmr_pool, 1);
    CTRACK_NAME("memory.pool.pmr_unsynchronized_mixed_batch");
    run_pmr_batch(pmr_pool, iterations);
  }

  const caors::size_class_pool_statistics statistics = pool.statistics();
  const std::size_t internal_fragmentation =
      statistics.peak_pooled_capacity_bytes_in_use > pooled_requested_bytes
          ? statistics.peak_pooled_capacity_bytes_in_use - pooled_requested_bytes
          : 0;
  std::cout << "memory.size_class_pool.sample pooled_requested=" << pooled_requested_bytes
            << " peak_capacity=" << statistics.peak_pooled_capacity_bytes_in_use
            << " internal_fragmentation=" << internal_fragmentation
            << " reserved=" << statistics.reserved_bytes << " chunks=" << statistics.chunk_count
            << " fallback=" << statistics.fallback_allocation_count << '\n';
}

} // namespace caors::benchmarks
