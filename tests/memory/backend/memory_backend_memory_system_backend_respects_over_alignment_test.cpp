// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "memory_backend_test_support.h"

using namespace caors::tests::memory_backend;
TEST_CASE("memory - system backend respects over-alignment", "[memory]") {
  auto& resource = caors::get_system_memory_resource();

  void* ptr = resource.allocate(128, 64);

  REQUIRE(ptr != nullptr);
  CHECK(reinterpret_cast<std::uintptr_t>(ptr) % 64 == 0);
  resource.deallocate(ptr, 128, 64);
}
