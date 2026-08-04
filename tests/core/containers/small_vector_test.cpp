// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/core/containers.h"
#include "caors/memory/tracking.h"

#include <catch2/catch_all.hpp>

#include <concepts>
#include <cstdint>
#include <ranges>
#include <stdexcept>
#include <type_traits>

namespace {

struct alignas(64) over_aligned_value {
  std::uint64_t value = 0;
};

} // namespace

static_assert(std::ranges::contiguous_range<caors::core::small_vector<int, 4>>);
static_assert(std::ranges::sized_range<caors::core::small_vector<int, 4>>);
static_assert(std::is_same_v<caors::small_vector<int, 4>, caors::core::small_vector<int, 4>>);
static_assert(caors::core::small_vector<int, 0>::inline_capacity() == 0);
static_assert(caors::core::small_vector<int, 4>::inline_capacity() == 4);

TEST_CASE("core small vector - empty container uses inline storage", "[core][small_vector]") {
  caors::tracking_resource tracking{&caors::get_system_memory_resource()};
  caors::core::small_vector<int, 4> values{&tracking};

  CHECK(values.empty());
  CHECK(values.size() == 0);
  CHECK(values.capacity() == 4);
  CHECK(values.begin() == values.end());
  CHECK(values.data() != nullptr);
  CHECK(values.uses_inline_storage());
  CHECK(values.resource() == &tracking);
  CHECK(values.get_allocator().resource() == &tracking);
  CHECK(tracking.statistics().allocation_count == 0);
}

TEST_CASE("core small vector - zero inline capacity is valid", "[core][small_vector]") {
  caors::core::small_vector<int, 0> values;

  CHECK(values.empty());
  CHECK(values.capacity() == 0);
  CHECK(values.begin() == values.end());
  CHECK(values.uses_inline_storage());
}

TEST_CASE("core small vector - factory binds the requested resource", "[core][small_vector]") {
  caors::tracking_resource tracking{&caors::get_system_memory_resource()};
  auto values = caors::make_small_vector<int, 3>(&tracking);

  CHECK(values.resource() == &tracking);
  CHECK(values.capacity() == 3);
  CHECK(values.uses_inline_storage());
}

TEST_CASE("core small vector - inline storage preserves element alignment",
          "[core][small_vector]") {
  caors::core::small_vector<over_aligned_value, 2> values;
  const auto address = reinterpret_cast<std::uintptr_t>(values.data());

  CHECK(address % alignof(over_aligned_value) == 0);
  CHECK(values.capacity() == 2);
}

TEST_CASE("core small vector - default construction captures default resource",
          "[core][small_vector]") {
  caors::tracking_resource tracking{&caors::get_system_memory_resource()};
  caors::memory_resource* previous = caors::set_default_memory_resource(&tracking);

  caors::core::small_vector<int, 2> values;
  caors::set_default_memory_resource(previous);

  CHECK(values.resource() == &tracking);
}

#if CAORS_HAS_EXCEPTIONS
TEST_CASE("core small vector - at reports an empty range", "[core][small_vector]") {
  const caors::core::small_vector<int, 2> values;
  CHECK_THROWS_AS(values.at(0), std::out_of_range);
}
#endif
