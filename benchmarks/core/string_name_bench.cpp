// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "support/benchmark_support.h"

#include <ctrack.hpp>

namespace caors::benchmarks {

void benchmark_string_name(std::size_t iterations) {
  {
    CTRACK_NAME("core.string_name.hot_lookup");
    for (std::size_t index = 0; index < iterations; ++index) {
      const caors::string_name name{"caors.benchmark.actor"};
      consume(name.empty() ? 0U : 1U);
    }
  }

  {
    CTRACK_NAME("core.string_name.compare");
    const caors::string_name left{"caors.benchmark.actor"};
    const caors::string_name right{"caors.benchmark.actor"};
    for (std::size_t index = 0; index < iterations; ++index) {
      consume(left == right ? 1U : 0U);
    }
  }
}

} // namespace caors::benchmarks
