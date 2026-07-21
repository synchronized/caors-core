// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "support/benchmark_support.h"

#include <ctrack.hpp>

#include <cstdlib>
#include <iostream>

namespace caors::benchmarks {

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

  benchmark_resource_io_fixture fixture;

  {
    CTRACK_NAME("resources.provider.read_memory_file_system");
    for (std::size_t index = 0; index < iterations; ++index) {
      caors::io::byte_buffer_result result = fixture.provider.read(benchmark_texture_guid);
      if (result.failed()) {
        std::cerr << "资源 provider 读取 benchmark 失败\n";
        std::exit(EXIT_FAILURE);
      }
      consume(static_cast<std::uintptr_t>(result.value.size()));
    }
  }

  {
    CTRACK_NAME("resources.json_loader.load_memory_file_system");
    for (std::size_t index = 0; index < iterations; ++index) {
      caors::resources::load_result<benchmark_texture_resource> result =
          fixture.loader.load(benchmark_texture_guid, fixture.cache.resource());
      if (result.failed()) {
        std::cerr << "资源 JSON loader benchmark 失败\n";
        std::exit(EXIT_FAILURE);
      }
      consume_address(result.value.get());
    }
  }

  {
    CTRACK_NAME("resources.load_handle_into.json_uncached");
    for (std::size_t index = 0; index < iterations; ++index) {
      (void)fixture.cache.erase(benchmark_texture_guid);
      caors::resources::handle_result<benchmark_texture_resource> result =
          caors::resources::load_handle_into(fixture.cache, fixture.loader, benchmark_texture_guid);
      if (result.failed()) {
        std::cerr << "资源 JSON load_handle benchmark 失败\n";
        std::exit(EXIT_FAILURE);
      }
      consume_address(result.value.get());
    }
  }
}

} // namespace caors::benchmarks
