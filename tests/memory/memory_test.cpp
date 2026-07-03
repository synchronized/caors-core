// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/core/containers.h"
#include "caors/memory.h"

#include <catch2/catch_all.hpp>

#include <cstdint>
#include <new>
#include <stdexcept>
#include <string_view>

namespace {

struct callback_memory_state {
  int allocate_count = 0;
  int deallocate_count = 0;
  std::size_t last_size = 0;
  std::size_t last_alignment = 0;
};

void* test_allocate(std::size_t size, std::size_t alignment, void* user_data) {
  auto* state = static_cast<callback_memory_state*>(user_data);
  ++state->allocate_count;
  state->last_size = size;
  state->last_alignment = alignment;
  return ::operator new(size, std::align_val_t{alignment});
}

void* test_failed_allocate(std::size_t size, std::size_t alignment, void* user_data) {
  auto* state = static_cast<callback_memory_state*>(user_data);
  ++state->allocate_count;
  state->last_size = size;
  state->last_alignment = alignment;
  return nullptr;
}

void test_deallocate(void* ptr, std::size_t size, std::size_t alignment, void* user_data) {
  auto* state = static_cast<callback_memory_state*>(user_data);
  ++state->deallocate_count;
  state->last_size = size;
  state->last_alignment = alignment;
  ::operator delete(ptr, std::align_val_t{alignment});
}

} // namespace

TEST_CASE("memory - system backend allocates storage", "[memory]") {
  auto& resource = caors::get_system_memory_resource();

  void* ptr = resource.allocate(32);

  REQUIRE(ptr != nullptr);
  resource.deallocate(ptr, 32);
}

TEST_CASE("memory - system backend respects over-alignment", "[memory]") {
  auto& resource = caors::get_system_memory_resource();

  void* ptr = resource.allocate(128, 64);

  REQUIRE(ptr != nullptr);
  CHECK(reinterpret_cast<std::uintptr_t>(ptr) % 64 == 0);
  resource.deallocate(ptr, 128, 64);
}

TEST_CASE("memory - invalid alignment is rejected", "[memory]") {
  auto& resource = caors::get_system_memory_resource();

  CHECK(resource.try_allocate(32, 3) == nullptr);
  CHECK(caors::try_allocate(32, 3) == nullptr);
#if CAORS_HAS_EXCEPTIONS
  CHECK_THROWS_AS(resource.allocate(32, 3), std::invalid_argument);
#endif
}

TEST_CASE("memory - try allocate reports backend failure", "[memory]") {
  callback_memory_state state;
  caors::memory::callback_memory_resource resource(&test_failed_allocate, &test_deallocate, &state);

  CHECK(resource.try_allocate(48, 32) == nullptr);
  CHECK(state.allocate_count == 1);
  CHECK(state.deallocate_count == 0);
  CHECK(state.last_size == 48);
  CHECK(state.last_alignment == 32);

#if CAORS_HAS_EXCEPTIONS
  CHECK_THROWS_AS(resource.allocate(48, 32), std::bad_alloc);
#endif
}

TEST_CASE("memory - callback backend uses callbacks", "[memory]") {
  callback_memory_state state;
  caors::memory::callback_memory_resource resource(&test_allocate, &test_deallocate, &state);

  void* ptr = resource.allocate(48, 32);

  REQUIRE(ptr != nullptr);
  CHECK(reinterpret_cast<std::uintptr_t>(ptr) % 32 == 0);
  CHECK(state.allocate_count == 1);
  CHECK(state.last_size == 48);
  CHECK(state.last_alignment == 32);

  resource.deallocate(ptr, 48, 32);

  CHECK(state.deallocate_count == 1);
  CHECK(state.last_size == 48);
  CHECK(state.last_alignment == 32);
}

TEST_CASE("memory - default resource can be replaced", "[memory]") {
  callback_memory_state state;
  caors::memory::callback_memory_resource resource(&test_allocate, &test_deallocate, &state);

  caors::memory_resource* old_resource = caors::set_default_memory_resource(&resource);

  void* ptr = caors::allocate(24, 32);
  caors::deallocate(ptr, 24, 32);
  caors::memory_resource* replaced_resource = caors::set_default_memory_resource(old_resource);

  CHECK(replaced_resource == &resource);
  CHECK(caors::get_default_memory_resource() == old_resource);
  CHECK(state.allocate_count == 1);
  CHECK(state.deallocate_count == 1);
}

TEST_CASE("memory - allocator adapts resources for standard containers", "[memory]") {
  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  caors::vector<int> values{caors::allocator<int>(&tracking)};

  values.push_back(1);
  values.push_back(2);
  values.push_back(3);

  CHECK(values[0] == 1);
  CHECK(values[1] == 2);
  CHECK(values[2] == 3);
  CHECK(tracking.statistics().allocation_count > 0);
  CHECK(tracking.statistics().bytes_allocated >= sizeof(int) * values.size());
}

TEST_CASE("memory - caors containers route through selected resource", "[memory]") {
  caors::tracking_resource tracking(&caors::get_system_memory_resource());

  caors::vector<int> values{caors::allocator<int>{&tracking}};
  values.push_back(1);
  values.push_back(2);
  values.push_back(3);

  caors::string text{caors::allocator<char>{&tracking}};
  text = "Ada";

  auto map = caors::make_hash_map<int, caors::string>(&tracking);
  map.emplace(7, caors::string{"seven", caors::allocator<char>{&tracking}});

  CHECK(values.size() == 3);
  CHECK(text == "Ada");
  CHECK(map.find(7) != map.end());
  CHECK(tracking.statistics().allocation_count > 0);
}

TEST_CASE("memory - caors container factories follow caors default resource", "[memory]") {
  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  caors::memory_resource* old_resource = caors::set_default_memory_resource(&tracking);

  {
    caors::vector<int> values = caors::make_vector<int>();
    values.reserve(8);
    values.push_back(42);

    caors::string text = caors::make_string();
    text.assign(std::string_view{"tracked"});

    CHECK(values[0] == 42);
    CHECK(text == "tracked");
  }

  caors::memory_resource* replaced_resource = caors::set_default_memory_resource(old_resource);

  CHECK(replaced_resource == &tracking);
  CHECK(tracking.statistics().allocation_count > 0);
  CHECK(tracking.statistics().bytes_in_use == 0);
}

TEST_CASE("memory - make and destroy use the selected resource", "[memory]") {
  struct object {
    explicit object(int value) : value(value) {}
    int value = 0;
  };

  caors::tracking_resource tracking(&caors::get_system_memory_resource());

  object* ptr = caors::make<object>(&tracking, 42);

  REQUIRE(ptr != nullptr);
  CHECK(ptr->value == 42);
  CHECK(tracking.statistics().allocation_count == 1);

  caors::destroy(ptr, &tracking);

  CHECK(tracking.statistics().deallocation_count == 1);
  CHECK(tracking.statistics().bytes_in_use == 0);
}

TEST_CASE("memory - tracking resource records allocation statistics", "[memory]") {
  caors::tracking_resource tracking(&caors::get_system_memory_resource());

  void* ptr = tracking.allocate(80, 16);

  auto stats = tracking.statistics();
  CHECK(stats.allocation_count == 1);
  CHECK(stats.bytes_allocated == 80);
  CHECK(stats.bytes_in_use == 80);
  CHECK(stats.peak_bytes_in_use == 80);

  tracking.deallocate(ptr, 80, 16);

  stats = tracking.statistics();
  CHECK(stats.deallocation_count == 1);
  CHECK(stats.bytes_deallocated == 80);
  CHECK(stats.bytes_in_use == 0);
}

TEST_CASE("memory - monotonic resource releases allocations on reset", "[memory]") {
  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  caors::monotonic_resource monotonic(64, &tracking);

  void* first = monotonic.allocate(16, 16);
  void* second = monotonic.allocate(16, 16);

  REQUIRE(first != nullptr);
  REQUIRE(second != nullptr);
  CHECK(first != second);
  CHECK(tracking.statistics().allocation_count > 0);
  CHECK(tracking.statistics().deallocation_count == 0);

  monotonic.deallocate(first, 16, 16);
  CHECK(tracking.statistics().deallocation_count == 0);

  monotonic.reset();

  CHECK(tracking.statistics().bytes_in_use == 0);
  CHECK(tracking.statistics().deallocation_count == tracking.statistics().allocation_count);
}

TEST_CASE("memory - fixed block pool reuses released blocks", "[memory]") {
  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  caors::fixed_block_pool pool(sizeof(int), alignof(int), 2, &tracking);

  void* first = pool.allocate(sizeof(int), alignof(int));
  void* second = pool.allocate(sizeof(int), alignof(int));

  REQUIRE(first != nullptr);
  REQUIRE(second != nullptr);
  CHECK(first != second);

  pool.deallocate(first, sizeof(int), alignof(int));
  void* reused = pool.allocate(sizeof(int), alignof(int));

  CHECK(reused == first);

  pool.deallocate(reused, sizeof(int), alignof(int));
  pool.deallocate(second, sizeof(int), alignof(int));
  pool.release();

  CHECK(tracking.statistics().bytes_in_use == 0);
}
