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
  const caors::meta::method assign_method = type.find_method("assign").single();
  const caors::meta::method_set select_methods = type.find_method("select");
  const caors::meta::method_set default_methods = type.find_method("add_default");
  const caors::meta::method_set conversion_methods = type.find_method("take_i64");
  if (!sum_method || !assign_method || !select_methods || !default_methods || !conversion_methods) {
    std::cerr << "benchmark 方法未注册\n";
    std::exit(EXIT_FAILURE);
  }

  int left = 3;
  int right = 5;
  const std::array<caors::argument, 2> argument_refs{caors::arg(left), caors::arg(right)};
  const std::array<caors::meta::value, 2> value_arguments{caors::meta::value{left},
                                                          caors::meta::value{right}};
  int unary = 3;
  const std::array<caors::argument, 1> unary_arguments{caors::arg(unary)};
  caors::meta::argument_match_options default_options{};
  default_options.allow_default_arguments = true;
  caors::meta::argument_match_options conversion_options{};
  conversion_options.conversion_policy = caors::meta::argument_conversion_policy::numeric_safe;

  {
    CTRACK_NAME("meta.method.direct_call");
    for (std::size_t index = 0; index < iterations; ++index) {
      consume(static_cast<std::uintptr_t>(actor.sum(left, right)));
    }
  }

  {
    CTRACK_NAME("meta.method.member_non_const_invoke_result");
    for (std::size_t index = 0; index < iterations; ++index) {
      unary = static_cast<int>(index);
      const caors::meta::invoke_result result =
          assign_method.invoke_result_of(instance, unary_arguments);
      const int* value = result.try_get<int>();
      consume(static_cast<std::uintptr_t>(value == nullptr ? 0 : *value));
    }
  }

  {
    CTRACK_NAME("meta.method.overload_match_exact");
    for (std::size_t index = 0; index < iterations; ++index) {
      const caors::meta::method_match_result match = select_methods.match(unary_arguments);
      consume(static_cast<std::uintptr_t>(match.matched()));
    }
  }

  {
    CTRACK_NAME("meta.method.overload_invoke_exact");
    for (std::size_t index = 0; index < iterations; ++index) {
      const caors::meta::invoke_result result =
          select_methods.invoke_result_of(instance, unary_arguments);
      const int* value = result.try_get<int>();
      consume(static_cast<std::uintptr_t>(value == nullptr ? 0 : *value));
    }
  }

  {
    CTRACK_NAME("meta.method.default_match");
    for (std::size_t index = 0; index < iterations; ++index) {
      const caors::meta::method_match_result match =
          default_methods.match(unary_arguments, default_options);
      consume(static_cast<std::uintptr_t>(match.matched()));
    }
  }

  {
    CTRACK_NAME("meta.method.default_invoke");
    for (std::size_t index = 0; index < iterations; ++index) {
      const caors::meta::invoke_result result =
          default_methods.invoke_result_of(instance, unary_arguments, default_options);
      const int* value = result.try_get<int>();
      consume(static_cast<std::uintptr_t>(value == nullptr ? 0 : *value));
    }
  }

  {
    CTRACK_NAME("meta.method.numeric_conversion_match");
    for (std::size_t index = 0; index < iterations; ++index) {
      const caors::meta::method_match_result match =
          conversion_methods.match(unary_arguments, conversion_options);
      consume(static_cast<std::uintptr_t>(match.matched()));
    }
  }

  {
    CTRACK_NAME("meta.method.numeric_conversion_invoke");
    for (std::size_t index = 0; index < iterations; ++index) {
      const caors::meta::invoke_result result =
          conversion_methods.invoke_result_of(instance, unary_arguments, conversion_options);
      const int* value = result.try_get<int>();
      consume(static_cast<std::uintptr_t>(value == nullptr ? 0 : *value));
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
