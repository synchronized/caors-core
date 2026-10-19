// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_BENCHMARKS_SUPPORT_BENCHMARK_SUPPORT_H_
#define CAORS_BENCHMARKS_SUPPORT_BENCHMARK_SUPPORT_H_

#include "caors/assets/json.h"
#include "caors/io.h"
#include "caors/memory/tracking.h"
#include "caors/meta.h"
#include "caors/resources.h"
#include "caors/serialization.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

namespace caors::benchmarks {

inline volatile std::uintptr_t benchmark_sink = 0;

inline void consume(std::uintptr_t value) noexcept { benchmark_sink = benchmark_sink ^ value; }

template <typename T> void consume_address(const T* value) noexcept {
  consume(reinterpret_cast<std::uintptr_t>(value));
}

template <typename T> void consume_value(const T& value) noexcept {
  consume_address(std::addressof(value));
}

[[nodiscard]] std::size_t parse_iterations(int argc, char** argv);

struct benchmark_actor {
  int id = 0;
  float health = 0.0F;
  bool active = false;
  caors::string name{};
  mutable int referenced_value = 0;

  [[nodiscard]] int health_int() const noexcept { return static_cast<int>(health); }
  void set_health_int(int value) noexcept { health = static_cast<float>(value); }
  [[nodiscard]] int& referenced_value_ref() const noexcept { return referenced_value; }
  void set_referenced_value(int value) noexcept { referenced_value = value; }
  [[nodiscard]] int readonly_id() const noexcept { return id; }
  [[nodiscard]] int sum(int left, int right) const noexcept { return id + left + right; }
  [[nodiscard]] int assign(int value) noexcept {
    id = value;
    return id;
  }
  [[nodiscard]] int select(int value) noexcept { return value + 1; }
  [[nodiscard]] int select(double value) const noexcept { return static_cast<int>(value) + 2; }
  [[nodiscard]] int add_default(int left, int right) const noexcept { return left + right; }
  [[nodiscard]] int take_i64(std::int64_t value) const noexcept { return static_cast<int>(value); }
};

inline constexpr caors::resource_guid benchmark_texture_guid{caors::guid::from_bytes(
    caors::guid::byte_array{0x8f, 0x1b, 0x2a, 0x44, 0x90, 0x0a, 0x45, 0x36, 0x92, 0x74, 0x71, 0xee,
                            0x4a, 0x51, 0x91, 0x09})};

inline constexpr std::string_view benchmark_actor_json{
    R"({"id":7,"health":95.5,"active":true,"name":"Ada"})"};

inline constexpr std::string_view benchmark_texture_json{R"({"width":256,"height":128})"};

class benchmark_texture_resource : public caors::resource {
public:
  using caors::resource::resource;

  int width = 0;
  int height = 0;
};

class benchmark_texture_loader final : public caors::resources::loader<benchmark_texture_resource> {
public:
  [[nodiscard]] caors::resources::load_result<benchmark_texture_resource>
  load(caors::resource_guid guid, caors::memory_resource* resource = nullptr) override;
};

struct benchmark_resource_io_fixture {
  explicit benchmark_resource_io_fixture(caors::memory_resource* resource = nullptr);

  caors::io::memory_file_system files;
  caors::resources::provider provider;
  caors::resource_cache<benchmark_texture_resource> cache;
  caors::resources::file_source source;
  caors::assets::json::loader<benchmark_texture_resource> loader;
};

[[nodiscard]] caors::serialization::type_schema make_benchmark_actor_schema();
[[nodiscard]] caors::variant make_benchmark_actor_variant_direct(const benchmark_actor& actor);
[[nodiscard]] caors::error
write_benchmark_actor_json_direct(caors::serialization::writer& writer,
                                  const benchmark_actor& actor,
                                  const caors::serialization::type_schema& schema);
void print_memory_statistics(std::string_view name, const caors::memory_statistics& statistics,
                             std::size_t iterations);
void ensure_benchmark_types_registered();

void benchmark_string_name(std::size_t iterations);
void benchmark_small_vector(std::size_t iterations);
void benchmark_meta_lookup(std::size_t iterations);
void benchmark_registry_resources(std::size_t iterations);
void benchmark_properties(std::size_t iterations);
void benchmark_method_calls(std::size_t iterations);
void benchmark_any(std::size_t iterations);
void benchmark_math(std::size_t iterations);
void benchmark_serialization(std::size_t iterations);
void benchmark_resources(std::size_t iterations);
void benchmark_resource_coroutines(std::size_t iterations);
void benchmark_task_coroutines(std::size_t iterations);
void benchmark_memory_pools(std::size_t iterations);
void print_meta_wrapper_layout_samples();
void print_meta_wrapper_registration_samples();
void print_allocation_samples();

} // namespace caors::benchmarks

#endif
