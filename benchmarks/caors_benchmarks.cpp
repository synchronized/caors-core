// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/core/string_name.h"
#include "caors/meta.h"
#include "caors/registration.h"
#include "caors/resources.h"
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

constexpr caors::resource_guid benchmark_texture_guid{caors::guid::from_bytes(
    caors::guid::byte_array{0x8f, 0x1b, 0x2a, 0x44, 0x90, 0x0a, 0x45, 0x36, 0x92, 0x74, 0x71, 0xee,
                            0x4a, 0x51, 0x91, 0x09})};

class benchmark_texture_resource : public caors::resource {
public:
  using caors::resource::resource;

  int width = 0;
  int height = 0;
};

class benchmark_texture_loader final : public caors::resources::loader<benchmark_texture_resource> {
public:
  [[nodiscard]] caors::resources::load_result<benchmark_texture_resource>
  load(caors::resource_guid guid, caors::memory_resource* resource = nullptr) override {
    auto value = caors::memory::make_unique<benchmark_texture_resource>(resource);
    if (value == nullptr) {
      return {{caors::error_domain::resources, caors::errc::out_of_memory}, {}};
    }

    value->set_guid(guid);
    value->width = 256;
    value->height = 128;
    return {{}, std::move(value)};
  }
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
  constexpr std::string_view actor_json{R"({"id":7,"health":95.5,"active":true,"name":"Ada"})"};

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

  {
    CTRACK_NAME("serialization.to_string_json");
    for (std::size_t index = 0; index < iterations; ++index) {
      caors::serialization::serialization_text_result result = caors::to_string(actor);
      if (result.failed()) {
        std::cerr << "JSON 序列化 benchmark 对象失败\n";
        std::exit(EXIT_FAILURE);
      }
      consume_value(result.value);
    }
  }

  {
    CTRACK_NAME("serialization.deserialize_json");
    for (std::size_t index = 0; index < iterations; ++index) {
      benchmark_actor target{};
      const caors::error status = caors::deserialize(target, actor_json);
      if (status.failed()) {
        std::cerr << "JSON 反序列化 benchmark 对象失败\n";
        std::exit(EXIT_FAILURE);
      }
      consume(static_cast<std::uintptr_t>(target.id));
    }
  }
}

void benchmark_resources(std::size_t iterations) {
  caors::resource_cache<benchmark_texture_resource> cache;
  benchmark_texture_loader loader;
  benchmark_texture_resource* texture = cache.create(benchmark_texture_guid);
  if (texture == nullptr) {
    std::cerr << "创建 benchmark 资源失败\n";
    std::exit(EXIT_FAILURE);
  }
  texture->width = 256;
  texture->height = 128;

  {
    CTRACK_NAME("resources.cache.find");
    for (std::size_t index = 0; index < iterations; ++index) {
      consume_address(cache.find(benchmark_texture_guid));
    }
  }

  {
    CTRACK_NAME("resources.load_handle_into.cached");
    for (std::size_t index = 0; index < iterations; ++index) {
      caors::resources::handle_result<benchmark_texture_resource> result =
          caors::resources::load_handle_into(cache, loader, benchmark_texture_guid);
      if (result.failed()) {
        std::cerr << "资源 cache hit benchmark 失败\n";
        std::exit(EXIT_FAILURE);
      }
      consume_address(result.value.get());
    }
  }

  {
    CTRACK_NAME("resources.load_handle_into.uncached");
    for (std::size_t index = 0; index < iterations; ++index) {
      (void)cache.erase(benchmark_texture_guid);
      caors::resources::handle_result<benchmark_texture_resource> result =
          caors::resources::load_handle_into(cache, loader, benchmark_texture_guid);
      if (result.failed()) {
        std::cerr << "资源加载 benchmark 失败\n";
        std::exit(EXIT_FAILURE);
      }
      consume_address(result.value.get());
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
  benchmark_resources(iterations);

  ctrack::result_print();
  return benchmark_sink == 0 ? EXIT_SUCCESS : EXIT_SUCCESS;
}
