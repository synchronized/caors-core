// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "support/benchmark_support.h"

#include "caors/core/containers.h"

#include <ctrack.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <iostream>

namespace caors::benchmarks {
namespace {

struct large_value {
  std::array<std::byte, 64> bytes{};
};

template <typename Container> void append_ints(Container& values, std::size_t count) {
  for (std::size_t index = 0; index < count; ++index) {
    values.push_back(static_cast<int>(index));
  }
  consume(values.empty() ? 0U : static_cast<std::uintptr_t>(values.back()));
}

template <typename Container> void append_large_values(Container& values) {
  for (std::size_t index = 0; index < 4; ++index) {
    values.emplace_back();
  }
  consume_address(values.data());
}

void print_small_vector_allocation_samples(std::size_t iterations) {
  const std::size_t sample_iterations = (std::min)(iterations, std::size_t{10'000});
  caors::tracking_resource tracking{&caors::get_system_memory_resource()};

  for (std::size_t iteration = 0; iteration < sample_iterations; ++iteration) {
    caors::small_vector<int, 8> values{&tracking};
    append_ints(values, 4);
  }
  print_memory_statistics("core.small_vector.inline4.allocations", tracking.statistics(),
                          sample_iterations);

  tracking.reset_statistics();
  for (std::size_t iteration = 0; iteration < sample_iterations; ++iteration) {
    caors::small_vector<int, 8> values{&tracking};
    append_ints(values, 12);
  }
  print_memory_statistics("core.small_vector.overflow12.allocations", tracking.statistics(),
                          sample_iterations);

  tracking.reset_statistics();
  for (std::size_t iteration = 0; iteration < sample_iterations; ++iteration) {
    caors::vector<int> values{caors::allocator<int>{&tracking}};
    append_ints(values, 4);
  }
  print_memory_statistics("core.vector.inline4_control.allocations", tracking.statistics(),
                          sample_iterations);
}

} // namespace

void benchmark_small_vector(std::size_t iterations) {
  std::cout << "\nSmall vector layout: small<int,8>=" << sizeof(caors::small_vector<int, 8>)
            << " vector<int>=" << sizeof(caors::vector<int>)
            << " small<large,4>=" << sizeof(caors::small_vector<large_value, 4>) << '\n';

  {
    CTRACK_NAME("core.small_vector.inline4");
    for (std::size_t iteration = 0; iteration < iterations; ++iteration) {
      caors::small_vector<int, 8> values;
      append_ints(values, 4);
    }
  }

  {
    CTRACK_NAME("core.vector.inline4_control");
    for (std::size_t iteration = 0; iteration < iterations; ++iteration) {
      caors::vector<int> values;
      append_ints(values, 4);
    }
  }

  {
    CTRACK_NAME("core.vector.inline4_reserved_control");
    for (std::size_t iteration = 0; iteration < iterations; ++iteration) {
      caors::vector<int> values;
      values.reserve(4);
      append_ints(values, 4);
    }
  }

  {
    CTRACK_NAME("core.array.inline4_fixed_control");
    for (std::size_t iteration = 0; iteration < iterations; ++iteration) {
      std::array<int, 8> values{};
      for (std::size_t index = 0; index < 4; ++index) {
        values[index] = static_cast<int>(index);
      }
      consume(static_cast<std::uintptr_t>(values[3]));
    }
  }

  {
    CTRACK_NAME("core.small_vector.overflow12");
    for (std::size_t iteration = 0; iteration < iterations; ++iteration) {
      caors::small_vector<int, 8> values;
      append_ints(values, 12);
    }
  }

  {
    CTRACK_NAME("core.vector.overflow12_control");
    for (std::size_t iteration = 0; iteration < iterations; ++iteration) {
      caors::vector<int> values;
      append_ints(values, 12);
    }
  }

  {
    CTRACK_NAME("core.vector.overflow12_reserved_control");
    for (std::size_t iteration = 0; iteration < iterations; ++iteration) {
      caors::vector<int> values;
      values.reserve(12);
      append_ints(values, 12);
    }
  }

  {
    CTRACK_NAME("core.small_vector.occasional_overflow");
    for (std::size_t iteration = 0; iteration < iterations; ++iteration) {
      caors::small_vector<int, 8> values;
      append_ints(values, iteration % 10 == 0 ? 12 : 4);
    }
  }

  {
    CTRACK_NAME("core.vector.occasional_overflow_control");
    for (std::size_t iteration = 0; iteration < iterations; ++iteration) {
      caors::vector<int> values;
      append_ints(values, iteration % 10 == 0 ? 12 : 4);
    }
  }

  {
    CTRACK_NAME("core.small_vector.overflow64");
    for (std::size_t iteration = 0; iteration < iterations; ++iteration) {
      caors::small_vector<int, 8> values;
      append_ints(values, 64);
    }
  }

  {
    CTRACK_NAME("core.vector.overflow64_control");
    for (std::size_t iteration = 0; iteration < iterations; ++iteration) {
      caors::vector<int> values;
      append_ints(values, 64);
    }
  }

  {
    CTRACK_NAME("core.vector.overflow64_reserved_control");
    for (std::size_t iteration = 0; iteration < iterations; ++iteration) {
      caors::vector<int> values;
      values.reserve(64);
      append_ints(values, 64);
    }
  }

  {
    CTRACK_NAME("core.small_vector.large_inline4");
    for (std::size_t iteration = 0; iteration < iterations; ++iteration) {
      caors::small_vector<large_value, 4> values;
      append_large_values(values);
    }
  }

  {
    CTRACK_NAME("core.vector.large4_control");
    for (std::size_t iteration = 0; iteration < iterations; ++iteration) {
      caors::vector<large_value> values;
      append_large_values(values);
    }
  }

  print_small_vector_allocation_samples(iterations);
}

} // namespace caors::benchmarks
