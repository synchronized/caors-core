// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "support/benchmark_support.h"

#include "caors/memory/fixed_block_pool.h"
#include "caors/memory/size_class_pool.h"

#include <ctrack.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <iostream>
#include <memory_resource>

namespace caors::benchmarks {
namespace {

constexpr std::array<std::size_t, 8> mixed_sizes{8, 24, 40, 72, 128, 256, 1024, 5000};
constexpr std::size_t pooled_requested_bytes = 1552;
constexpr std::size_t fixed_size = 64;
constexpr std::size_t burst_width = 128;

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

void run_caors_fixed(caors::memory_resource& resource, std::size_t iterations) {
  for (std::size_t iteration = 0; iteration < iterations; ++iteration) {
    void* ptr = resource.allocate(fixed_size);
    consume_address(ptr);
    resource.deallocate(ptr, fixed_size);
  }
}

void run_pmr_fixed(std::pmr::memory_resource& resource, std::size_t iterations) {
  for (std::size_t iteration = 0; iteration < iterations; ++iteration) {
    void* ptr = resource.allocate(fixed_size);
    consume_address(ptr);
    resource.deallocate(ptr, fixed_size);
  }
}

void run_caors_burst(caors::memory_resource& resource, std::size_t burst_count) {
  std::array<void*, burst_width> pointers{};
  for (std::size_t burst = 0; burst < burst_count; ++burst) {
    for (std::size_t index = 0; index < pointers.size(); ++index) {
      const std::size_t size = mixed_sizes[index % mixed_sizes.size()];
      pointers[index] = resource.allocate(size);
      consume_address(pointers[index]);
    }
    for (std::size_t index = pointers.size(); index != 0; --index) {
      const std::size_t size = mixed_sizes[(index - 1) % mixed_sizes.size()];
      resource.deallocate(pointers[index - 1], size);
    }
  }
}

void run_pmr_burst(std::pmr::memory_resource& resource, std::size_t burst_count) {
  std::array<void*, burst_width> pointers{};
  for (std::size_t burst = 0; burst < burst_count; ++burst) {
    for (std::size_t index = 0; index < pointers.size(); ++index) {
      const std::size_t size = mixed_sizes[index % mixed_sizes.size()];
      pointers[index] = resource.allocate(size);
      consume_address(pointers[index]);
    }
    for (std::size_t index = pointers.size(); index != 0; --index) {
      const std::size_t size = mixed_sizes[(index - 1) % mixed_sizes.size()];
      resource.deallocate(pointers[index - 1], size);
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
    CTRACK_NAME("memory.pool.system_fixed");
    run_caors_fixed(system, iterations);
  }

  {
    caors::fixed_block_pool fixed_pool{fixed_size, caors::default_memory_alignment, 64, &system};
    run_caors_fixed(fixed_pool, 1);
    CTRACK_NAME("memory.pool.manual_fixed_fixed");
    run_caors_fixed(fixed_pool, iterations);
  }

  {
    caors::size_class_pool_resource fixed_pool{{}, &system};
    run_caors_fixed(fixed_pool, 1);
    CTRACK_NAME("memory.pool.size_class_fixed");
    run_caors_fixed(fixed_pool, iterations);
  }

  {
    std::pmr::unsynchronized_pool_resource fixed_pool{};
    run_pmr_fixed(fixed_pool, 1);
    CTRACK_NAME("memory.pool.pmr_unsynchronized_fixed");
    run_pmr_fixed(fixed_pool, iterations);
  }

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

  const std::size_t burst_count = std::max<std::size_t>(1, iterations / burst_width);
  {
    CTRACK_NAME("memory.pool.system_burst");
    run_caors_burst(system, burst_count);
  }

  caors::size_class_pool_resource burst_pool{{}, &system};
  {
    CTRACK_NAME("memory.pool.size_class_burst");
    run_caors_burst(burst_pool, burst_count);
  }

  {
    std::pmr::pool_options pmr_options{};
    pmr_options.max_blocks_per_chunk = 64;
    pmr_options.largest_required_pool_block = 4096;
    std::pmr::unsynchronized_pool_resource pmr_pool{pmr_options};
    CTRACK_NAME("memory.pool.pmr_unsynchronized_burst");
    run_pmr_burst(pmr_pool, burst_count);
  }

  const caors::size_class_pool_statistics burst_statistics = burst_pool.statistics();
  std::cout << "memory.size_class_pool.burst peak_capacity="
            << burst_statistics.peak_pooled_capacity_bytes_in_use
            << " reserved=" << burst_statistics.reserved_bytes
            << " chunks=" << burst_statistics.chunk_count
            << " fallback=" << burst_statistics.fallback_allocation_count << '\n';

  const std::size_t low_reuse_cycles = std::max<std::size_t>(1, iterations / 10);
  {
    CTRACK_NAME("memory.pool.system_low_reuse");
    run_caors_batch(system, low_reuse_cycles);
  }

  {
    CTRACK_NAME("memory.pool.size_class_low_reuse");
    for (std::size_t cycle = 0; cycle < low_reuse_cycles; ++cycle) {
      caors::size_class_pool_resource transient_pool{{}, &system};
      run_caors_batch(transient_pool, 1);
    }
  }

  {
    CTRACK_NAME("memory.pool.pmr_unsynchronized_low_reuse");
    for (std::size_t cycle = 0; cycle < low_reuse_cycles; ++cycle) {
      std::pmr::pool_options pmr_options{};
      pmr_options.max_blocks_per_chunk = 64;
      pmr_options.largest_required_pool_block = 4096;
      std::pmr::unsynchronized_pool_resource transient_pool{pmr_options};
      run_pmr_batch(transient_pool, 1);
    }
  }
}

} // namespace caors::benchmarks
