// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/core/containers.h"

#include <catch2/catch_all.hpp>

TEST_CASE("core containers - vector uses caors allocator", "[core]") {
  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  caors::vector<int> values{caors::allocator<int>{&tracking}};

  values.push_back(1);
  values.push_back(2);

  CHECK(values[0] == 1);
  CHECK(values[1] == 2);
  CHECK(tracking.statistics().allocation_count > 0);
}

TEST_CASE("core containers - string uses caors allocator", "[core]") {
  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  caors::string text{caors::allocator<char>{&tracking}};

  text.assign(64, 'x');

  CHECK(text.size() == 64);
  CHECK(tracking.statistics().allocation_count > 0);
}

TEST_CASE("core containers - hash containers use caors allocator factories", "[core]") {
  caors::tracking_resource tracking(&caors::get_system_memory_resource());

  auto names = caors::make_hash_map<int, caors::string>(&tracking);
  names.emplace(1, caors::string{"one", caors::allocator<char>{&tracking}});

  auto ids = caors::make_hash_set<int>(&tracking);
  ids.insert(1);

  REQUIRE(names.find(1) != names.end());
  CHECK(names.find(1)->second == "one");
  CHECK(ids.contains(1));
  CHECK(tracking.statistics().allocation_count > 0);
}
