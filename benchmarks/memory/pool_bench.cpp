// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "support/benchmark_support.h"

#include "caors/memory/fixed_block_pool.h"
#include "caors/memory/size_class_pool.h"

#include <ctrack.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory_resource>
#include <vector>

namespace caors::benchmarks {
namespace {

constexpr std::array<std::size_t, 8> mixed_sizes{8, 24, 40, 72, 128, 256, 1024, 5000};
constexpr std::array<std::size_t, 7> pooled_sizes{8, 24, 40, 72, 128, 256, 1024};
constexpr std::size_t pooled_requested_bytes = 1552;
constexpr std::size_t fixed_size = 64;
constexpr std::size_t burst_width = 128;
constexpr std::size_t maximum_growth_width = 256;
constexpr std::size_t latency_batch_width = 16;

struct latency_summary {
  std::uint64_t p50_nanoseconds = 0;
  std::uint64_t p95_nanoseconds = 0;
  std::uint64_t maximum_nanoseconds = 0;
};

[[nodiscard]] latency_summary summarize_latency(std::vector<std::uint64_t> samples) {
  std::ranges::sort(samples);
  const std::size_t last = samples.size() - 1;
  return latency_summary{samples[last * 50 / 100], samples[last * 95 / 100], samples[last]};
}

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

void run_caors_variable_burst(caors::memory_resource& resource, std::size_t width,
                              std::size_t burst_count) {
  std::array<void*, maximum_growth_width> pointers{};
  for (std::size_t burst = 0; burst < burst_count; ++burst) {
    for (std::size_t index = 0; index < width; ++index) {
      const std::size_t size = pooled_sizes[index % pooled_sizes.size()];
      pointers[index] = resource.allocate(size);
      consume_address(pointers[index]);
    }
    for (std::size_t index = width; index != 0; --index) {
      const std::size_t size = pooled_sizes[(index - 1) % pooled_sizes.size()];
      resource.deallocate(pointers[index - 1], size);
    }
  }
}

[[nodiscard]] latency_summary sample_caors_allocation_latency(caors::memory_resource& resource,
                                                              std::size_t allocation_count) {
  std::vector<void*> pointers(allocation_count);
  std::vector<std::size_t> sizes(allocation_count);
  std::vector<std::uint64_t> samples(allocation_count / latency_batch_width);

  for (std::size_t batch = 0; batch < samples.size(); ++batch) {
    const std::size_t begin_index = batch * latency_batch_width;
    const auto begin = std::chrono::steady_clock::now();
    for (std::size_t offset = 0; offset < latency_batch_width; ++offset) {
      const std::size_t index = begin_index + offset;
      sizes[index] = mixed_sizes[index % mixed_sizes.size()];
      pointers[index] = resource.allocate(sizes[index]);
    }
    const auto end = std::chrono::steady_clock::now();
    samples[batch] =
        static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count()) /
        latency_batch_width;
    consume_address(pointers[begin_index + latency_batch_width - 1]);
  }

  for (std::size_t index = allocation_count; index != 0; --index) {
    resource.deallocate(pointers[index - 1], sizes[index - 1]);
  }
  return summarize_latency(std::move(samples));
}

[[nodiscard]] latency_summary sample_pmr_allocation_latency(std::pmr::memory_resource& resource,
                                                            std::size_t allocation_count) {
  std::vector<void*> pointers(allocation_count);
  std::vector<std::size_t> sizes(allocation_count);
  std::vector<std::uint64_t> samples(allocation_count / latency_batch_width);

  for (std::size_t batch = 0; batch < samples.size(); ++batch) {
    const std::size_t begin_index = batch * latency_batch_width;
    const auto begin = std::chrono::steady_clock::now();
    for (std::size_t offset = 0; offset < latency_batch_width; ++offset) {
      const std::size_t index = begin_index + offset;
      sizes[index] = mixed_sizes[index % mixed_sizes.size()];
      pointers[index] = resource.allocate(sizes[index]);
    }
    const auto end = std::chrono::steady_clock::now();
    samples[batch] =
        static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count()) /
        latency_batch_width;
    consume_address(pointers[begin_index + latency_batch_width - 1]);
  }

  for (std::size_t index = allocation_count; index != 0; --index) {
    resource.deallocate(pointers[index - 1], sizes[index - 1]);
  }
  return summarize_latency(std::move(samples));
}

void print_latency(std::string_view name, const latency_summary& summary,
                   std::size_t allocation_count) {
  std::cout << "memory.pool.latency name=" << name
            << " sample_batches=" << allocation_count / latency_batch_width
            << " batch_width=" << latency_batch_width << " allocations=" << allocation_count
            << " p50_ns=" << summary.p50_nanoseconds << " p95_ns=" << summary.p95_nanoseconds
            << " max_ns=" << summary.maximum_nanoseconds << '\n';
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

  const std::size_t latency_allocation_count =
      std::clamp<std::size_t>(iterations, 256, 8192) / latency_batch_width * latency_batch_width;
  print_latency("system", sample_caors_allocation_latency(system, latency_allocation_count),
                latency_allocation_count);

  caors::size_class_pool_resource latency_pool{{}, &system};
  print_latency("size_class",
                sample_caors_allocation_latency(latency_pool, latency_allocation_count),
                latency_allocation_count);

  std::pmr::pool_options latency_pmr_options{};
  latency_pmr_options.max_blocks_per_chunk = 64;
  latency_pmr_options.largest_required_pool_block = 4096;
  std::pmr::unsynchronized_pool_resource latency_pmr_pool{latency_pmr_options};
  print_latency("pmr_unsynchronized",
                sample_pmr_allocation_latency(latency_pmr_pool, latency_allocation_count),
                latency_allocation_count);

  caors::size_class_pool_resource growth_pool{{}, &system};
  run_caors_variable_burst(growth_pool, 32, 1);
  const caors::size_class_pool_statistics low_peak = growth_pool.statistics();
  run_caors_variable_burst(growth_pool, 32, std::max<std::size_t>(1, iterations / 100));
  const caors::size_class_pool_statistics repeated_low = growth_pool.statistics();
  run_caors_variable_burst(growth_pool, maximum_growth_width, 1);
  const caors::size_class_pool_statistics high_peak = growth_pool.statistics();
  run_caors_variable_burst(growth_pool, 128, std::max<std::size_t>(1, iterations / 100));
  const caors::size_class_pool_statistics settled = growth_pool.statistics();
  std::cout << "memory.size_class_pool.capacity_growth low_peak_reserved="
            << low_peak.reserved_bytes << " repeated_low_reserved=" << repeated_low.reserved_bytes
            << " high_peak_reserved=" << high_peak.reserved_bytes
            << " settled_reserved=" << settled.reserved_bytes
            << " high_peak_chunks=" << high_peak.chunk_count
            << " settled_chunks=" << settled.chunk_count << '\n';
}

} // namespace caors::benchmarks
