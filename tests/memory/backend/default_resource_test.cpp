// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "memory_backend_test_support.h"

using namespace caors::tests::memory_backend;
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

TEST_CASE("memory - library resource remains a process resource compatibility alias", "[memory]") {
  CHECK(&caors::library_resource() == &caors::process_resource());
}
