// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "memory_backend_test_support.h"

using namespace caors::tests::memory_backend;
TEST_CASE("memory - invalid alignment is rejected", "[memory]") {
  auto& resource = caors::get_system_memory_resource();

  CHECK(resource.try_allocate(32, 3) == nullptr);
  CHECK(caors::try_allocate(32, 3) == nullptr);
#if CAORS_HAS_EXCEPTIONS
  CHECK_THROWS_AS(resource.allocate(32, 3), std::invalid_argument);
#endif
}
