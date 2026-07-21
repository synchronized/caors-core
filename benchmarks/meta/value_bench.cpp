// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "support/benchmark_support.h"

#include <ctrack.hpp>

namespace caors::benchmarks {

void benchmark_any(std::size_t iterations) {
  benchmark_actor actor{7, 95.5F, true, "Ada"};

  {
    CTRACK_NAME("meta.any_ref.from_and_try_get");
    for (std::size_t index = 0; index < iterations; ++index) {
      caors::any_ref ref = caors::any_ref::from(actor);
      benchmark_actor* value = ref.try_get_mutable<benchmark_actor>();
      consume_address(value);
    }
  }

  {
    CTRACK_NAME("meta.any.from_scalar");
    for (std::size_t index = 0; index < iterations; ++index) {
      caors::any value = caors::any::from(static_cast<int>(index));
      consume_value(value);
    }
  }
}

} // namespace caors::benchmarks
