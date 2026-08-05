// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "support/benchmark_support.h"

#include "caors/memory/monotonic.h"
#include "caors/memory/size_class_pool.h"
#include "caors/memory/tracking.h"
#include "caors/registration.h"

#include <ctrack.hpp>

#include <cstddef>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

namespace caors::benchmarks {
namespace {

constexpr std::size_t registry_resource_type_count = 8;

template <std::size_t Index> struct registry_resource_type {
  int value = static_cast<int>(Index);
  [[nodiscard]] int get_value() const noexcept { return value; }
};

struct registry_resource_fixture {
  explicit registry_resource_fixture(memory_resource& layer_resource)
      : layer(host.create_layer(meta::registry_layer_options{
            .internal_resource = &layer_resource,
        })),
        context(layer->get_registry()) {}

  meta::registry host{};
  std::unique_ptr<meta::registry_layer> layer;
  meta::context context;
};

template <std::size_t Index> [[nodiscard]] bool register_resource_type(meta::context& context) {
  using value_type = registry_resource_type<Index>;
  const std::string name = "caors.benchmark.registry.resource." + std::to_string(Index);
  return registration::type_<value_type>(context, name)
      .meta()
      .property("value", &value_type::value)
      .method("get_value", &value_type::get_value)
      .serialization()
      .field("value")
      .commit()
      .succeeded();
}

template <std::size_t... Indices>
[[nodiscard]] bool register_resource_types(meta::context& context,
                                           std::index_sequence<Indices...>) {
  return (register_resource_type<Indices>(context) && ...);
}

template <std::size_t... Indices>
void query_resource_types(meta::registry& registry, std::size_t iterations,
                          std::index_sequence<Indices...>) {
  for (std::size_t iteration = 0; iteration < iterations; ++iteration) {
    ((consume(registry.find_type(meta::resolve<registry_resource_type<Indices>>().id()).registered()
                  ? 1U
                  : 0U)),
     ...);
  }
}

[[nodiscard]] bool register_and_publish(registry_resource_fixture& fixture) {
  if (!register_resource_types(fixture.context,
                               std::make_index_sequence<registry_resource_type_count>{})) {
    return false;
  }
  return fixture.host.publish_layer(*fixture.layer).succeeded();
}

[[nodiscard]] bool unload(registry_resource_fixture& fixture) {
  if (fixture.host.detach_layer(*fixture.layer).failed()) {
    return false;
  }
  fixture.layer.reset();
  return true;
}

void print_memory_sample(std::string_view strategy, std::string_view phase,
                         const memory_statistics& statistics) {
  std::cout << "meta.registry.resource strategy=" << strategy << " phase=" << phase
            << " allocations=" << statistics.allocation_count
            << " deallocations=" << statistics.deallocation_count
            << " bytes_allocated=" << statistics.bytes_allocated
            << " bytes_in_use=" << statistics.bytes_in_use
            << " peak_bytes=" << statistics.peak_bytes_in_use << '\n';
}

void benchmark_system_resource(std::size_t iterations) {
  tracking_resource tracking{&get_system_memory_resource()};
  registry_resource_fixture fixture{tracking};
  {
    CTRACK_NAME("meta.registry.resource.system.register");
    if (!register_and_publish(fixture)) {
      std::cerr << "system registry resource benchmark 注册失败\n";
      return;
    }
  }
  print_memory_sample("system", "live", tracking.statistics());
  {
    CTRACK_NAME("meta.registry.resource.system.query");
    query_resource_types(fixture.host, iterations,
                         std::make_index_sequence<registry_resource_type_count>{});
  }
  {
    CTRACK_NAME("meta.registry.resource.system.unload");
    if (!unload(fixture)) {
      std::cerr << "system registry resource benchmark 卸载失败\n";
      return;
    }
  }
  print_memory_sample("system", "released", tracking.statistics());
}

void benchmark_monotonic_resource(std::size_t iterations) {
  tracking_resource tracking{&get_system_memory_resource()};
  monotonic_resource monotonic{4096, &tracking};
  registry_resource_fixture fixture{monotonic};
  {
    CTRACK_NAME("meta.registry.resource.monotonic.register");
    if (!register_and_publish(fixture)) {
      std::cerr << "monotonic registry resource benchmark 注册失败\n";
      return;
    }
  }
  print_memory_sample("monotonic", "live", tracking.statistics());
  {
    CTRACK_NAME("meta.registry.resource.monotonic.query");
    query_resource_types(fixture.host, iterations,
                         std::make_index_sequence<registry_resource_type_count>{});
  }
  {
    CTRACK_NAME("meta.registry.resource.monotonic.unload");
    if (!unload(fixture)) {
      std::cerr << "monotonic registry resource benchmark 卸载失败\n";
      return;
    }
  }
  print_memory_sample("monotonic", "unloaded", tracking.statistics());
  monotonic.reset();
  print_memory_sample("monotonic", "released", tracking.statistics());
}

void benchmark_pool_resource(std::size_t iterations) {
  tracking_resource tracking{&get_system_memory_resource()};
  size_class_pool_resource pool{&tracking};
  registry_resource_fixture fixture{pool};
  {
    CTRACK_NAME("meta.registry.resource.pool.register");
    if (!register_and_publish(fixture)) {
      std::cerr << "pool registry resource benchmark 注册失败\n";
      return;
    }
  }
  print_memory_sample("pool", "live", tracking.statistics());
  {
    CTRACK_NAME("meta.registry.resource.pool.query");
    query_resource_types(fixture.host, iterations,
                         std::make_index_sequence<registry_resource_type_count>{});
  }
  {
    CTRACK_NAME("meta.registry.resource.pool.unload");
    if (!unload(fixture)) {
      std::cerr << "pool registry resource benchmark 卸载失败\n";
      return;
    }
  }
  print_memory_sample("pool", "unloaded", tracking.statistics());
  pool.release();
  print_memory_sample("pool", "released", tracking.statistics());
}

} // namespace

void benchmark_registry_resources(std::size_t iterations) {
  benchmark_system_resource(iterations);
  benchmark_monotonic_resource(iterations);
  benchmark_pool_resource(iterations);
}

} // namespace caors::benchmarks
