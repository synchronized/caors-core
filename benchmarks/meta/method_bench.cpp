// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "support/benchmark_support.h"

#include <ctrack.hpp>

#include <array>
#include <cstdlib>
#include <iostream>

namespace caors::benchmarks {

void benchmark_method_calls(std::size_t iterations) {
  benchmark_actor actor{7, 95.5F, true, "Ada"};
  const caors::meta::instance instance = caors::meta::view(actor);
  const caors::meta::type type = caors::meta::find<benchmark_actor>();
  const caors::meta::method sum_method = type.find_method("sum").single();
  if (!sum_method) {
    std::cerr << "benchmark 方法未注册\n";
    std::exit(EXIT_FAILURE);
  }

  int left = 3;
  int right = 5;
  const std::array<caors::argument, 2> argument_refs{caors::arg(left), caors::arg(right)};
  const std::array<caors::meta::value, 2> value_arguments{caors::meta::value{left},
                                                          caors::meta::value{right}};

  {
    CTRACK_NAME("meta.method.direct_call");
    for (std::size_t index = 0; index < iterations; ++index) {
      consume(static_cast<std::uintptr_t>(actor.sum(left, right)));
    }
  }

  {
    CTRACK_NAME("meta.method.invoke_result_of_any");
    for (std::size_t index = 0; index < iterations; ++index) {
      const caors::meta::invoke_result result =
          sum_method.invoke_result_of(instance, argument_refs);
      const int* value = result.try_get<int>();
      consume(static_cast<std::uintptr_t>(value == nullptr ? 0 : *value));
    }
  }

  {
    CTRACK_NAME("meta.method.invoke_value");
    for (std::size_t index = 0; index < iterations; ++index) {
      const caors::meta::invoke_value_result result = sum_method.invoke(instance, value_arguments);
      consume(static_cast<std::uintptr_t>(result.value().as_i64()));
    }
  }

  {
    CTRACK_NAME("meta.instance.call_value_span");
    for (std::size_t index = 0; index < iterations; ++index) {
      const caors::meta::invoke_value_result result = instance.call("sum", value_arguments);
      consume(static_cast<std::uintptr_t>(result.value().as_i64()));
    }
  }

  {
    CTRACK_NAME("meta.instance.call_value_template");
    for (std::size_t index = 0; index < iterations; ++index) {
      const caors::meta::invoke_value_result result = instance.call("sum", left, right);
      consume(static_cast<std::uintptr_t>(result.value().as_i64()));
    }
  }

  {
    CTRACK_NAME("meta.instance.call_any_template");
    for (std::size_t index = 0; index < iterations; ++index) {
      const caors::any result = instance.call_any("sum", left, right);
      const int* value = result.try_get<int>();
      consume(static_cast<std::uintptr_t>(value == nullptr ? 0 : *value));
    }
  }
}

} // namespace caors::benchmarks
