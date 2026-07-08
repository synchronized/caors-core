// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "memory_backend_test_support.h"

using namespace caors::tests::memory_backend;
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
