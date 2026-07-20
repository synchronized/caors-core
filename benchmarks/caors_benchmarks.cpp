// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/core/string_name.h"
#include "caors/meta.h"
#include "caors/registration.h"
#include "caors/serialization.h"

#include <ctrack.hpp>

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <string_view>

namespace {

inline volatile std::uintptr_t benchmark_sink = 0;

void consume(std::uintptr_t value) noexcept { benchmark_sink = benchmark_sink ^ value; }

template <typename T> void consume_address(const T* value) noexcept {
  consume(reinterpret_cast<std::uintptr_t>(value));
}

template <typename T> void consume_value(const T& value) noexcept {
  consume_address(std::addressof(value));
}

std::size_t parse_iterations(int argc, char** argv) {
  constexpr std::size_t default_iterations = 100'000;
  if (argc <= 1) {
    return default_iterations;
  }

  std::string_view argument = argv[1];
  if (argument == "-h" || argument == "--help") {
    std::cout << "用法: caors_benchmarks [iterations]\n";
    std::cout << "默认 iterations = " << default_iterations << '\n';
    std::exit(EXIT_SUCCESS);
  }

  std::size_t iterations = default_iterations;
  const char* begin = argument.data();
  const char* end = begin + argument.size();
  const std::from_chars_result result = std::from_chars(begin, end, iterations);
  if (result.ec != std::errc{} || result.ptr != end || iterations == 0) {
    std::cerr << "无效 iterations 参数: " << argument << '\n';
    std::exit(EXIT_FAILURE);
  }

  return iterations;
}

struct benchmark_actor {
  int id = 0;
  float health = 0.0F;
  bool active = false;
  caors::string name{};

  [[nodiscard]] int health_int() const noexcept { return static_cast<int>(health); }
  void set_health_int(int value) noexcept { health = static_cast<float>(value); }
  [[nodiscard]] int sum(int left, int right) const noexcept { return id + left + right; }
};

void ensure_benchmark_types_registered() {
  static bool registered = false;
  if (registered) {
    return;
  }

  const caors::error registration_status =
      caors::registration::type_<benchmark_actor>("caors.benchmark.actor")
          .meta()
          .property("id", &benchmark_actor::id)
          .property("health", &benchmark_actor::health)
          .property("active", &benchmark_actor::active)
          .property("name", &benchmark_actor::name)
          .property("health_int", &benchmark_actor::health_int, &benchmark_actor::set_health_int)
          .method("sum", &benchmark_actor::sum)
          .serialization()
          .field("id")
          .field("health")
          .field("active")
          .field("name")
          .commit();
  if (registration_status.failed()) {
    std::cerr << "注册 benchmark 类型失败\n";
    std::exit(EXIT_FAILURE);
  }

  registered = true;
}

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

void benchmark_meta_lookup(std::size_t iterations) {
  {
    CTRACK_NAME("meta.resolve");
    for (std::size_t index = 0; index < iterations; ++index) {
      const caors::meta::type type = caors::meta::resolve<benchmark_actor>();
      consume(type.valid() ? 1U : 0U);
    }
  }

  {
    CTRACK_NAME("meta.find_registered");
    for (std::size_t index = 0; index < iterations; ++index) {
      const caors::meta::type type = caors::meta::find<benchmark_actor>();
      consume(type.registered() ? 1U : 0U);
    }
  }
}

void benchmark_properties(std::size_t iterations) {
  benchmark_actor actor{7, 95.5F, true, "Ada"};
  const caors::meta::instance instance = caors::meta::view(actor);
  const caors::meta::type type = caors::meta::find<benchmark_actor>();
  const caors::meta::property id_property = type.find_property("id");
  const caors::meta::property health_property = type.find_property("health_int");
  if (!id_property || !health_property) {
    std::cerr << "benchmark 属性未注册\n";
    std::exit(EXIT_FAILURE);
  }

  {
    CTRACK_NAME("meta.property.member_get_set_typed");
    for (std::size_t index = 0; index < iterations; ++index) {
      const int value = static_cast<int>(index);
      if (!id_property.try_set(instance, value)) {
        std::cerr << "写入 member 属性失败\n";
        std::exit(EXIT_FAILURE);
      }
      const std::optional<int> current = id_property.try_get<int>(instance);
      consume(static_cast<std::uintptr_t>(current.value_or(0)));
    }
  }

  {
    CTRACK_NAME("meta.property.member_get_set_value");
    for (std::size_t index = 0; index < iterations; ++index) {
      const int value = static_cast<int>(index);
      if (!id_property.try_set_value(instance, caors::meta::value{value})) {
        std::cerr << "写入 member 属性 value 失败\n";
        std::exit(EXIT_FAILURE);
      }
      const caors::meta::value current = id_property.get_value(instance);
      consume(static_cast<std::uintptr_t>(current.as_i64()));
    }
  }

  {
    CTRACK_NAME("meta.instance.get_set_value");
    for (std::size_t index = 0; index < iterations; ++index) {
      const int value = static_cast<int>(index);
      if (!instance.set("id", caors::meta::value{value})) {
        std::cerr << "写入 instance 属性 value 失败\n";
        std::exit(EXIT_FAILURE);
      }
      const caors::meta::value current = instance.get("id");
      consume(static_cast<std::uintptr_t>(current.as_i64()));
    }
  }

  {
    CTRACK_NAME("meta.property.accessor_get_set_any");
    for (std::size_t index = 0; index < iterations; ++index) {
      const int value = static_cast<int>(index % 100);
      const caors::any boxed = caors::any::from(value);
      if (!health_property.try_set_any(instance, boxed)) {
        std::cerr << "写入 getter/setter 属性失败\n";
        std::exit(EXIT_FAILURE);
      }
      caors::any current = health_property.get_any(instance);
      const int* current_value = current.try_get<int>();
      consume(static_cast<std::uintptr_t>(current_value == nullptr ? 0 : *current_value));
    }
  }

  {
    CTRACK_NAME("meta.property.accessor_get_set_value");
    for (std::size_t index = 0; index < iterations; ++index) {
      const int value = static_cast<int>(index % 100);
      if (!health_property.try_set_value(instance, caors::meta::value{value})) {
        std::cerr << "写入 getter/setter 属性 value 失败\n";
        std::exit(EXIT_FAILURE);
      }
      caors::meta::value current = health_property.get_value(instance);
      consume(static_cast<std::uintptr_t>(current.as_i64()));
    }
  }
}

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

void benchmark_serialization(std::size_t iterations) {
  const benchmark_actor actor{7, 95.5F, true, "Ada"};

  {
    CTRACK_NAME("serialization.to_value");
    for (std::size_t index = 0; index < iterations; ++index) {
      caors::serialization::variant_result result = caors::to_value(actor);
      if (result.failed()) {
        std::cerr << "序列化 benchmark 对象失败\n";
        std::exit(EXIT_FAILURE);
      }
      consume_value(result.value);
    }
  }
}

} // namespace

int main(int argc, char** argv) {
  const std::size_t iterations = parse_iterations(argc, argv);
  ensure_benchmark_types_registered();

  std::cout << "CAORS benchmark iterations: " << iterations << '\n';

  benchmark_string_name(iterations);
  benchmark_meta_lookup(iterations);
  benchmark_properties(iterations);
  benchmark_method_calls(iterations);
  benchmark_any(iterations);
  benchmark_serialization(iterations);

  ctrack::result_print();
  return benchmark_sink == 0 ? EXIT_SUCCESS : EXIT_SUCCESS;
}
