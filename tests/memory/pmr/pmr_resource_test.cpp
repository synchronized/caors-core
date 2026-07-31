// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/core/containers.h"
#include "caors/memory.h"

#include <catch2/catch_all.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory_resource>
#include <new>
#include <string>
#include <vector>

namespace {

class recording_pmr_resource final : public std::pmr::memory_resource {
public:
  explicit recording_pmr_resource(bool fail = false) noexcept : fail_(fail) {}

  std::size_t allocation_count = 0;
  std::size_t deallocation_count = 0;
  std::size_t last_size = 0;
  std::size_t last_alignment = 0;

private:
  void* do_allocate(std::size_t size, std::size_t alignment) override {
    if (fail_) {
#if CAORS_HAS_EXCEPTIONS
      throw std::bad_alloc{};
#else
      std::abort();
#endif
    }

    ++allocation_count;
    last_size = size;
    last_alignment = alignment;
    return std::pmr::new_delete_resource()->allocate(size, alignment);
  }

  void do_deallocate(void* ptr, std::size_t size, std::size_t alignment) override {
    ++deallocation_count;
    last_size = size;
    last_alignment = alignment;
    std::pmr::new_delete_resource()->deallocate(ptr, size, alignment);
  }

  bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
    return this == &other;
  }

  bool fail_ = false;
};

struct alignas(64) over_aligned_value {
  int value = 0;
};

} // namespace

TEST_CASE("memory - pmr resource view routes standard containers through caors", "[memory][pmr]") {
  caors::tracking_resource tracking{&caors::get_system_memory_resource()};
  caors::pmr_resource_view view{tracking};

  {
    std::pmr::vector<int> values{&view};
    values.push_back(1);
    values.push_back(2);

    std::pmr::string text{&view};
    text.assign(128, 'a');

    CHECK(values[1] == 2);
    CHECK(text.size() == 128);
    CHECK(tracking.statistics().allocation_count > 0);
  }

  CHECK(tracking.statistics().bytes_in_use == 0);
}

TEST_CASE("memory - pmr resource view preserves extended alignment", "[memory][pmr]") {
  caors::tracking_resource tracking{&caors::get_system_memory_resource()};
  caors::pmr_resource_view view{tracking};

  std::pmr::polymorphic_allocator<over_aligned_value> allocator{&view};
  over_aligned_value* value = allocator.allocate(1);

  REQUIRE(value != nullptr);
  CHECK(reinterpret_cast<std::uintptr_t>(value) % alignof(over_aligned_value) == 0);

  allocator.deallocate(value, 1);
  CHECK(tracking.statistics().bytes_in_use == 0);
}

TEST_CASE("memory - pmr resource view equality follows the caors resource", "[memory][pmr]") {
  caors::tracking_resource first{&caors::get_system_memory_resource()};
  caors::tracking_resource second{&caors::get_system_memory_resource()};
  caors::pmr_resource_view first_view{first};
  caors::pmr_resource_view same_view{first};
  caors::pmr_resource_view second_view{second};

  CHECK(first_view.is_equal(same_view));
  CHECK_FALSE(first_view.is_equal(second_view));
  CHECK_FALSE(first_view.is_equal(*std::pmr::new_delete_resource()));
}

TEST_CASE("memory - caors resource view routes caors containers through pmr", "[memory][pmr]") {
  recording_pmr_resource resource;
  caors::caors_resource_view view{resource};

  {
    caors::vector<int> values{caors::allocator<int>{&view}};
    values.push_back(7);
    values.push_back(9);

    REQUIRE(values.size() == 2);
    CHECK(values[1] == 9);
    CHECK(resource.allocation_count > 0);
  }

  CHECK(resource.deallocation_count == resource.allocation_count);
}

TEST_CASE("memory - caors resource view preserves size and alignment", "[memory][pmr]") {
  recording_pmr_resource resource;
  caors::caors_resource_view view{resource};

  void* ptr = view.try_allocate(sizeof(over_aligned_value), alignof(over_aligned_value));

  REQUIRE(ptr != nullptr);
  CHECK(resource.last_size == sizeof(over_aligned_value));
  CHECK(resource.last_alignment == alignof(over_aligned_value));

  view.deallocate(ptr, sizeof(over_aligned_value), alignof(over_aligned_value));
  CHECK(resource.deallocation_count == 1);
}

TEST_CASE("memory - caors resource view preserves caors zero size normalization", "[memory][pmr]") {
  recording_pmr_resource resource;
  caors::caors_resource_view view{resource};

  void* ptr = view.try_allocate(0);

  REQUIRE(ptr != nullptr);
  CHECK(resource.last_size == 1);

  view.deallocate(ptr, 0);
  CHECK(resource.last_size == 1);
  CHECK(resource.deallocation_count == 1);
}

#if CAORS_HAS_EXCEPTIONS
TEST_CASE("memory - caors resource view converts pmr allocation failure", "[memory][pmr]") {
  recording_pmr_resource resource{true};
  caors::caors_resource_view view{resource};

  CHECK(view.try_allocate(64) == nullptr);
  CHECK_THROWS_AS(view.allocate(64), std::bad_alloc);
}
#endif

TEST_CASE("memory - caors resource view equality follows the pmr resource", "[memory][pmr]") {
  recording_pmr_resource first;
  recording_pmr_resource second;
  caors::caors_resource_view first_view{first};
  caors::caors_resource_view same_view{first};
  caors::caors_resource_view second_view{second};

  CHECK(first_view.is_equal(same_view));
  CHECK_FALSE(first_view.is_equal(second_view));
  CHECK_FALSE(first_view.is_equal(caors::get_system_memory_resource()));
}
