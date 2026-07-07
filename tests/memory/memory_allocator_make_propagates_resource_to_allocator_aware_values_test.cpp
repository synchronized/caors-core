// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/core/containers.h"
#include "caors/memory.h"

#include <catch2/catch_all.hpp>

#include <string_view>

TEST_CASE("memory - make propagates resource to allocator-aware values", "[memory]") {
  caors::tracking_resource tracking(&caors::get_system_memory_resource());

  auto* values = caors::make<caors::vector<caors::string>>(&tracking);

  REQUIRE(values != nullptr);
  CHECK(values->get_allocator().resource() == &tracking);

  values->reserve(2);
  values->emplace_back("alpha");

  REQUIRE(values->size() == 1);
  CHECK(values->front() == "alpha");
  CHECK(values->front().get_allocator().resource() == &tracking);
  CHECK(tracking.statistics().allocation_count >= 2);

  caors::destroy(values, &tracking);

  CHECK(tracking.statistics().bytes_in_use == 0);
  CHECK(tracking.statistics().allocation_count == tracking.statistics().deallocation_count);
}
