// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "memory_backend_test_support.h"

using namespace caors::tests::memory_backend;
TEST_CASE("memory - system backend allocates storage", "[memory]") {
  auto& resource = caors::get_system_memory_resource();

  void* ptr = resource.allocate(32);

  REQUIRE(ptr != nullptr);
  resource.deallocate(ptr, 32);
}
