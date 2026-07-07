// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/core/containers.h"
#include "caors/memory/tracking.h"

#include <catch2/catch_all.hpp>

TEST_CASE("core containers - string uses caors allocator", "[core]") {
  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  caors::string text{caors::allocator<char>{&tracking}};

  text.assign(64, 'x');

  CHECK(text.size() == 64);
  CHECK(tracking.statistics().allocation_count > 0);
}
