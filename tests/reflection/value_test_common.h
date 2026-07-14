// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#pragma once

#include "caors/adapters/stl.h"
#include "caors/memory.h"
#include "caors/meta/value/view_factory.h"
#include "caors/reflection.h"

#include <catch2/catch_all.hpp>

#include <array>
#include <cstddef>
#include <memory>
#include <new>
#include <optional>
#include <span>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace caors::tests::reflection {

struct reflected_value_holder {
  int value = 0;
  const int readonly = 7;
};

struct move_only_value {
  std::unique_ptr<int> value{};
};

struct any_resource_value {
  int value = 17;
};

struct tracked_small_trivial {
  int value;

  static void* operator new(std::size_t size) {
    ++allocation_count;
    return ::operator new(size);
  }

  static void operator delete(void* ptr) noexcept {
    ++deallocation_count;
    ::operator delete(ptr);
  }

  inline static int allocation_count = 0;
  inline static int deallocation_count = 0;
};

struct tracked_large_trivial {
  int value;
  unsigned char padding[128]{};

  static void* operator new(std::size_t size) {
    ++allocation_count;
    return ::operator new(size);
  }

  static void operator delete(void* ptr) noexcept {
    ++deallocation_count;
    ::operator delete(ptr);
  }

  inline static int allocation_count = 0;
  inline static int deallocation_count = 0;
};

struct unregistered_nested_vector_holder {
  std::vector<std::vector<int>> values{};
};

struct indirect_value_holder {
  int value = 7;
  int* pointer = &value;
  std::optional<int> optional = 11;
  std::unique_ptr<int> unique = std::make_unique<int>(13);
  std::weak_ptr<int> weak{};
};

[[nodiscard]] inline bool type_result_ok(caors::error result) noexcept {
  return result.succeeded() || result.value() == caors::errc::duplicate_id;
}

template <typename T> void require_registered_type(std::string_view name) {
  REQUIRE(type_result_ok(caors::meta::class_<T>(name).result()));
}

inline void require_value_test_meta() {
  require_registered_type<any_resource_value>("caors.test.reflection.any_resource_value");
}

[[nodiscard]] inline caors::reflection::field require_reflected_field(std::string_view type_name,
                                                                      std::string_view field_name) {
  auto reflected = caors::reflection::find(type_name);
  REQUIRE(reflected);
  auto field = reflected.find_field(field_name);
  REQUIRE(field);
  return field;
}

} // namespace caors::tests::reflection
