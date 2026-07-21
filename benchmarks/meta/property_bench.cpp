// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "support/benchmark_support.h"

#include <ctrack.hpp>

#include <cstdlib>
#include <iostream>
#include <optional>

namespace caors::benchmarks {

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

} // namespace caors::benchmarks
