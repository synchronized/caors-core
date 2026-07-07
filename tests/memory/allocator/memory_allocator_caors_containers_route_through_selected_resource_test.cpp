// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/core/containers.h"
#include "caors/memory.h"

#include <catch2/catch_all.hpp>

#include <string_view>

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
