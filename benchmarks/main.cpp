// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "support/benchmark_support.h"

#include <ctrack.hpp>

#include <cstdlib>
#include <iostream>

int main(int argc, char** argv) {
  const std::size_t iterations = caors::benchmarks::parse_iterations(argc, argv);
  caors::benchmarks::ensure_benchmark_types_registered();

  std::cout << "CAORS benchmark iterations: " << iterations << '\n';

  caors::benchmarks::benchmark_string_name(iterations);
  caors::benchmarks::benchmark_meta_lookup(iterations);
  caors::benchmarks::benchmark_properties(iterations);
  caors::benchmarks::benchmark_method_calls(iterations);
  caors::benchmarks::benchmark_any(iterations);
  caors::benchmarks::benchmark_serialization(iterations);
  caors::benchmarks::benchmark_resources(iterations);
  caors::benchmarks::print_allocation_samples();

  ctrack::result_print();
  return EXIT_SUCCESS;
}
