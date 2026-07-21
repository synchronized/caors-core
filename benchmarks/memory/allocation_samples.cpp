// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "support/benchmark_support.h"

#include <cstdlib>
#include <iostream>

namespace caors::benchmarks {

void print_allocation_samples() {
  constexpr std::size_t iterations = 1'000;
  const benchmark_actor actor{7, 95.5F, true, "Ada"};

  std::cout << "\nAllocation samples (iterations: " << iterations << ")\n";

  {
    caors::tracking_resource tracking{&caors::get_system_memory_resource()};
    caors::serialization::context context{};
    context.resolvers.memory.resource = &tracking;
    for (std::size_t index = 0; index < iterations; ++index) {
      caors::serialization::variant_result result = caors::to_value(context, actor);
      if (result.failed()) {
        std::cerr << "统计 to_value 分配失败\n";
        std::exit(EXIT_FAILURE);
      }
      consume_value(result.value);
    }
    print_memory_statistics("serialization.to_value", tracking.statistics(), iterations);
  }

  {
    caors::tracking_resource tracking{&caors::get_system_memory_resource()};
    {
      caors::string output{caors::allocator<char>{&tracking}};
      output.reserve(benchmark_actor_json.size());
      for (std::size_t index = 0; index < iterations; ++index) {
        output.clear();
        const caors::error status = caors::serialization::write_json_to(actor, output);
        if (status.failed()) {
          std::cerr << "统计 write_json_to 分配失败\n";
          std::exit(EXIT_FAILURE);
        }
        consume(static_cast<std::uintptr_t>(output.size()));
      }
    }
    print_memory_statistics("serialization.write_json_to_reused_buffer", tracking.statistics(),
                            iterations);
  }

  {
    caors::tracking_resource tracking{&caors::get_system_memory_resource()};
    caors::memory_resource* previous = caors::set_default_memory_resource(&tracking);
    for (std::size_t index = 0; index < iterations; ++index) {
      caors::serialization::variant_result result =
          caors::serialization::parse_json(benchmark_actor_json);
      if (result.failed()) {
        std::cerr << "统计 parse_json 分配失败\n";
        (void)caors::set_default_memory_resource(previous);
        std::exit(EXIT_FAILURE);
      }
      consume_value(result.value);
    }
    (void)caors::set_default_memory_resource(previous);
    print_memory_statistics("serialization.parse_json", tracking.statistics(), iterations);
  }

  {
    caors::tracking_resource tracking{&caors::get_system_memory_resource()};
    {
      benchmark_resource_io_fixture fixture{&tracking};
      for (std::size_t index = 0; index < iterations; ++index) {
        (void)fixture.cache.erase(benchmark_texture_guid);
        caors::resources::handle_result<benchmark_texture_resource> result =
            caors::resources::load_handle_into(fixture.cache, fixture.loader,
                                               benchmark_texture_guid);
        if (result.failed()) {
          std::cerr << "统计资源 JSON 加载分配失败\n";
          std::exit(EXIT_FAILURE);
        }
        consume_address(result.value.get());
      }
      fixture.cache.clear();
    }
    print_memory_statistics("resources.load_handle_into.json_uncached", tracking.statistics(),
                            iterations);
  }
}

} // namespace caors::benchmarks
