// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#pragma once

#include "caors/resources.h"

#include "caors/core/containers.h"
#include "caors/io.h"
#include "caors/memory/tracking.h"
#include "caors/reflection.h"
#include "caors/serialization.h"

#include <catch2/catch_all.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>

namespace caors::tests::resources {

constexpr caors::resource_guid test_guid_a{caors::guid::from_bytes(
    caors::guid::byte_array{0x6c, 0xd2, 0xda, 0x3d, 0xb5, 0x49, 0x47, 0x56, 0x8b, 0x8b, 0x0c, 0x84,
                            0x29, 0x3a, 0xf4, 0x10})};

constexpr caors::resource_guid test_guid_b{caors::guid::from_bytes(
    caors::guid::byte_array{0x0c, 0x2f, 0x82, 0xf8, 0x2b, 0x3e, 0x4f, 0x77, 0x96, 0x5c, 0x4f, 0x2d,
                            0x1c, 0x4a, 0x90, 0x7a})};

[[nodiscard]] inline std::filesystem::path make_temp_dir() {
  static std::atomic_uint64_t counter{0};
  const auto suffix = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
                      "_" + std::to_string(counter.fetch_add(1));
  return std::filesystem::temp_directory_path() / ("caors_resources_test_" + suffix);
}

[[nodiscard]] inline std::string path_string(const std::filesystem::path& path) {
  return path.string();
}

class texture_resource : public caors::resource {
public:
  using caors::resource::resource;
};

class image_texture_resource : public texture_resource {
public:
  using texture_resource::texture_resource;
};

class observed_resource : public caors::resource {
public:
  using caors::resource::resource;

  ~observed_resource() override { ++destroyed; }

  inline static int destroyed = 0;
};

class serialized_resource : public caors::resource {
public:
  using caors::resource::resource;

  int width = 0;
};

class counting_texture_loader final : public caors::resource_loader<texture_resource> {
public:
  [[nodiscard]] caors::resource_load_result<texture_resource>
  load(caors::resource_guid guid, caors::memory_resource* resource = nullptr) override {
    ++load_count;
    auto loaded = caors::memory::make_unique<texture_resource>(resource, guid);
    return {{}, std::move(loaded)};
  }

  int load_count = 0;
};

class counting_texture_saver final : public caors::resource_saver<texture_resource> {
public:
  [[nodiscard]] caors::error save(const texture_resource& value) override {
    ++save_count;
    saved_guid = value.guid();
    return {};
  }

  int save_count = 0;
  caors::resource_guid saved_guid{};
};

class read_only_source final : public caors::resource_source {
public:
  [[nodiscard]] bool exists(caors::io::path_view location) const override {
    return !location.empty();
  }

  [[nodiscard]] caors::byte_buffer_result
  read(caors::io::path_view location, caors::memory_resource* resource = nullptr) override {
    if (location.empty()) {
      return {{caors::error_domain::resources, caors::errc::invalid_argument}, {}};
    }

    caors::byte_buffer bytes = caors::make_byte_buffer(resource);
    return {{}, std::move(bytes)};
  }
};

[[nodiscard]] inline bool reflection_result_ok(caors::error result) noexcept {
  return result.succeeded() || result.value() == caors::errc::duplicate_reflection;
}

[[nodiscard]] inline bool serialization_result_ok(caors::error result) noexcept {
  return result.succeeded() || result.value() == caors::errc::duplicate_component;
}

inline void require_serialized_resource_metadata() {
  REQUIRE(reflection_result_ok(
      ((void)caors::meta::type_<serialized_resource>("caors.test.resources.serialized_resource"),
       caors::reflection::class_<serialized_resource>())
          .field("width", &serialized_resource::width)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::class_<serialized_resource>().commit()));
}

} // namespace caors::tests::resources
