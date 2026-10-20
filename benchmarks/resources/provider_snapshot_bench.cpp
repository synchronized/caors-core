// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/assets/asset_manager.h"
#include "caors/io.h"
#include "caors/tasks/execution_context.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <numeric>
#include <vector>

namespace {
using clock_type = std::chrono::steady_clock;
std::ofstream raw_samples;
std::int64_t ns(clock_type::time_point begin) {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(clock_type::now() - begin).count();
}
void require(bool value) {
  if (!value) {
    std::cerr << "快照基准校验失败\n";
    std::exit(1);
  }
}
caors::resource_guid make_guid(std::uint64_t value) {
  caors::guid::byte_array bytes{};
  for (std::size_t index = 0; index < 8; ++index)
    bytes[index] = static_cast<std::uint8_t>(value >> (index * 8));
  bytes[15] = 0x51;
  return caors::resource_guid{caors::guid::from_bytes(bytes)};
}
// 仅统计明确使用本 allocator 的分配；不覆盖 STL 默认分配或进程总内存。
class measured_resource final : public caors::memory_resource {
public:
  std::size_t active = 0, peak = 0, allocations = 0, allocated = 0;
  void window() {
    peak = active;
    allocations = allocated = 0;
  }

private:
  void* do_try_allocate(std::size_t size, std::size_t alignment) noexcept override {
    auto* ptr = caors::get_system_memory_resource().try_allocate(size, alignment);
    if (ptr) {
      active += size;
      allocated += size;
      ++allocations;
      peak = (std::max)(peak, active);
    }
    return ptr;
  }
  void do_deallocate(void* ptr, std::size_t size, std::size_t alignment) noexcept override {
    active -= size;
    caors::get_system_memory_resource().deallocate(ptr, size, alignment);
  }
  bool do_is_equal(const caors::memory_resource& other) const noexcept override {
    return &other == this;
  }
};
class counted_source final : public caors::resources::source {
public:
  explicit counted_source(std::shared_ptr<caors::io::file_system> files)
      : source_(std::move(files)) {}
  mutable std::size_t freezes = 0;
  bool exists(caors::io::location_view location) const override { return source_.exists(location); }
  caors::io::byte_buffer_result read(caors::io::location_view location,
                                     caors::memory_resource* memory) override {
    return source_.read(location, memory);
  }
  caors::result<std::shared_ptr<caors::io::file_system>>
  freeze_routes(caors::io::detail::snapshot_context& context) const override {
    ++freezes;
    return source_.freeze_routes(context);
  }

private:
  caors::resources::file_source source_;
};
struct fixture {
  measured_resource memory;
  std::vector<std::shared_ptr<caors::io::mount_file_system>> routers;
  std::vector<std::unique_ptr<counted_source>> sources;
  caors::resources::provider provider{&memory};
  std::string location;
  fixture(std::size_t count, std::size_t backend_count, bool nested, bool long_path) {
    location = (nested ? "bench://assets/data/" : "data/") + std::string(long_path ? 240 : 4, 'a');
    for (std::size_t index = 0; index < backend_count; ++index) {
      auto router = std::make_shared<caors::io::mount_file_system>();
      require(!router->mount("data", std::make_shared<caors::io::memory_file_system>()));
      routers.push_back(router);
      std::shared_ptr<caors::io::file_system> root = router;
      if (nested) {
        auto resolver = std::make_shared<caors::io::file_system_resolver>();
        require(!resolver->mount("bench://assets", root));
        root = std::move(resolver);
      }
      sources.push_back(std::make_unique<counted_source>(root));
    }
    for (std::size_t index = 0; index < count; ++index)
      require(!provider.bind(make_guid(index + 1), *sources[index % backend_count], location));
  }
  void reset() {
    memory.window();
    for (auto& source : sources)
      source->freezes = 0;
  }
  std::size_t freezes() const {
    std::size_t count = 0;
    for (auto& source : sources)
      count += source->freezes;
    return count;
  }
};
struct sample {
  std::int64_t time, update;
  std::size_t allocations, allocated, retained, peak, freezes;
};
void report(std::size_t size, std::size_t sources, bool nested, bool long_path, const char* mode,
            const std::vector<sample>& rows) {
  if (raw_samples) {
    for (std::size_t index = 0; index < rows.size(); ++index) {
      const auto& row = rows[index];
      raw_samples << size << ',' << sources << ',' << nested << ',' << long_path << ',' << mode
                  << ',' << index << ',' << row.time << ',' << row.update << ',' << row.allocations
                  << ',' << row.allocated << ',' << row.retained << ',' << row.peak << ','
                  << row.freezes << '\n';
    }
  }
  std::vector<std::int64_t> times;
  for (auto row : rows)
    times.push_back(row.time);
  std::sort(times.begin(), times.end());
  auto percentile = [&](std::size_t percent) {
    return times[((times.size() - 1) * percent + 99) / 100];
  };
  auto maximum = [&](auto member) {
    std::size_t value = 0;
    for (auto row : rows)
      value = (std::max)(value, row.*member);
    return value;
  };
  std::int64_t update = 0;
  for (auto row : rows)
    update = (std::max)(update, row.update);
  std::cout << size << ',' << sources << ',' << nested << ',' << long_path << ',' << mode << ','
            << rows.size() << ',' << percentile(50) << ',' << percentile(95) << ','
            << percentile(99) << ',' << times.back() << ',' << update << ','
            << maximum(&sample::allocations) << ',' << maximum(&sample::allocated) << ','
            << maximum(&sample::retained) << ',' << maximum(&sample::peak) << ','
            << maximum(&sample::freezes) << '\n';
}
void captures(std::size_t count, std::size_t backend_count, bool nested, bool long_path,
              int samples) {
  fixture f{count, backend_count, nested, long_path};
  {
    f.reset();
    const auto before = f.memory.active;
    const auto begin = clock_type::now();
    auto snapshot = f.provider.snapshot();
    const auto elapsed = ns(begin);
    require(snapshot->resolve(make_guid(1)).succeeded());
    report(count, backend_count, nested, long_path, "first_capture",
           {{elapsed, 0, f.memory.allocations, f.memory.allocated, f.memory.active - before,
             f.memory.peak, f.freezes()}});
  }
  for (int mode = 0; mode < 4; ++mode) {
    std::vector<std::shared_ptr<const caors::resources::provider_snapshot>> retained;
    std::vector<sample> rows;
    for (int index = -3; index < samples; ++index) {
      auto begin = clock_type::now();
      if (mode == 1)
        require(!f.provider.rebind(make_guid(1), *f.sources.back(), f.location));
      if (mode == 2) {
        require(f.routers.front()->unmount("data"));
        require(
            !f.routers.front()->mount("data", std::make_shared<caors::io::memory_file_system>()));
      }
      const auto update = mode == 1 || mode == 2 ? ns(begin) : 0;
      f.reset();
      const auto baseline = f.memory.active;
      begin = clock_type::now();
      auto snapshot = f.provider.snapshot();
      const auto elapsed = ns(begin);
      require(snapshot->resolve(make_guid(1)).succeeded());
      if (index >= 0)
        rows.push_back({elapsed, update, f.memory.allocations, f.memory.allocated,
                        f.memory.active - baseline, f.memory.peak, f.freezes()});
      if (mode == 3) {
        retained.push_back(std::move(snapshot));
        if (retained.size() > 4)
          retained.erase(retained.begin());
      }
    }
    report(count, backend_count, nested, long_path,
           mode == 0   ? "capture"
           : mode == 1 ? "rebind"
           : mode == 2 ? "remount"
                       : "retain4",
           rows);
  }
}
class bench_resource final : public caors::resource {
public:
  using resource::resource;
};
class bench_loader final : public caors::resources::async_loader<bench_resource> {
public:
  caors::resources::load_completion<bench_resource> pending;
  caors::error start(caors::resources::async_load_context,
                     caors::resources::load_completion<bench_resource> receipt) override {
    pending = std::move(receipt);
    return {};
  }
};
void request_paths(std::size_t count, int samples) {
  fixture f{count, 1, false, false};
  caors::resources::cache<bench_resource> cache;
  bench_loader loader;
  caors::tasks::execution_context worker, completion;
  caors::assets::asset_manager manager{{worker.get_executor(), completion.get_executor()}};
  manager.database().set_provider(f.provider);
  const caors::string_name type{"bench.snapshot"};
  require(!manager.register_type(cache, loader, type));
  require(cache.create(make_guid(1), make_guid(1)) != nullptr);
  require(!manager.manifest().bind(make_guid(1), "data/a", type));
  require(!manager.manifest().bind(make_guid(2), "data/b", type));
  require(!manager.manifest().bind(make_guid(3), "data/c", type));
  require(cache.create(make_guid(3), make_guid(3)) != nullptr);
  require(manager.dependency_graph().add(make_guid(3), make_guid(2)));
  auto pending = manager.request_load<bench_resource>(make_guid(2));
  require(worker.drain() == 1);
  for (int mode = 0; mode < 8; ++mode) {
    std::vector<sample> rows;
    for (int index = -3; index < samples; ++index) {
      f.reset();
      const auto before = f.memory.active;
      const auto begin = clock_type::now();
      if (mode < 2) {
        auto request = manager.request_load<bench_resource>(make_guid(mode == 0 ? 1 : 2));
        require(mode == 0 ? request.loaded() : !request.done());
      } else {
        const auto root = make_guid(mode >= 6 ? 3 : mode >= 4 ? 2 : 1);
        auto request = mode % 2 == 0 ? manager.request_preload<bench_resource>(root)
                                     : manager.request_preload(root);
        require(mode < 4 ? request.loaded() : !request.done());
      }
      const auto elapsed = ns(begin);
      if (index >= 0)
        rows.push_back({elapsed, 0, f.memory.allocations, f.memory.allocated,
                        f.memory.active - before, f.memory.peak, f.freezes()});
    }
    const char* names[]{"cache_hit",
                        "inflight_join",
                        "typed_preload_hit",
                        "dispatched_preload_hit",
                        "typed_preload_inflight",
                        "dispatched_preload_inflight",
                        "typed_preload_dependency",
                        "dispatched_preload_dependency"};
    report(count, 1, false, false, names[mode], rows);
  }
  (void)pending.cancel();
  loader.pending = {};
  (void)completion.drain();
}
} // namespace
int main(int argc, char** argv) {
  const int samples = argc > 1 ? std::atoi(argv[1]) : 31;
  const auto only_size =
      argc > 2 ? static_cast<std::size_t>(std::strtoull(argv[2], nullptr, 10)) : 0;
  if (samples < 3 || (only_size && only_size != 1000 && only_size != 10000 && only_size != 100000))
    return 2;
  if (argc > 3) {
    raw_samples.open(argv[3]);
    if (!raw_samples)
      return 2;
    raw_samples
        << "bindings,sources,nested,long_path,mode,index,time_ns,update_ns,explicit_allocations,"
           "explicit_bytes,new_retained_bytes,explicit_peak_including_fixture,freeze_calls\n";
  }
  std::cout << "bindings,sources,nested,long_path,mode,samples,p50_ns,p95_ns,p99_ns,max_ns,update_"
               "max_ns,explicit_allocations_max,explicit_bytes_max,new_retained_bytes_max,explicit_"
               "peak_including_fixture_max,freeze_calls_max\n";
  for (std::size_t count : {1000, 10000, 100000}) {
    if (only_size && only_size != count)
      continue;
    for (int config = 0; config < 4; ++config)
      captures(count, config & 1 ? 8 : 1, config & 2, config & 1, samples);
    request_paths(count, samples);
  }
}
