// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "memory_backend_test_support.h"

using namespace caors::tests::memory_backend;
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
