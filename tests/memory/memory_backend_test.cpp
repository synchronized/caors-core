// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/memory.h"

#include <catch2/catch_all.hpp>

#include <cstdint>
#include <new>
#include <stdexcept>

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
