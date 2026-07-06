// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/core/containers.h"
#include "caors/memory.h"

#include <catch2/catch_all.hpp>

#include <string_view>

TEST_CASE("memory - allocator adapts resources for standard containers", "[memory]") {
  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  caors::vector<int> values{caors::allocator<int>(&tracking)};

  values.push_back(1);
  values.push_back(2);
  values.push_back(3);

  CHECK(values[0] == 1);
  CHECK(values[1] == 2);
  CHECK(values[2] == 3);
  CHECK(tracking.statistics().allocation_count > 0);
  CHECK(tracking.statistics().bytes_allocated >= sizeof(int) * values.size());
}

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

TEST_CASE("memory - caors container factories follow caors default resource", "[memory]") {
  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  caors::memory_resource* old_resource = caors::set_default_memory_resource(&tracking);

  {
    caors::vector<int> values = caors::make_vector<int>();
    values.reserve(8);
    values.push_back(42);

    caors::string text = caors::make_string();
    text.assign(std::string_view{"tracked"});

    CHECK(values[0] == 42);
    CHECK(text == "tracked");
  }

  caors::memory_resource* replaced_resource = caors::set_default_memory_resource(old_resource);

  CHECK(replaced_resource == &tracking);
  CHECK(tracking.statistics().allocation_count > 0);
  CHECK(tracking.statistics().bytes_in_use == 0);
}

TEST_CASE("memory - make and destroy use the selected resource", "[memory]") {
  struct object {
    explicit object(int value) : value(value) {}
    int value = 0;
  };

  caors::tracking_resource tracking(&caors::get_system_memory_resource());

  object* ptr = caors::make<object>(&tracking, 42);

  REQUIRE(ptr != nullptr);
  CHECK(ptr->value == 42);
  CHECK(tracking.statistics().allocation_count == 1);

  caors::destroy(ptr, &tracking);

  CHECK(tracking.statistics().deallocation_count == 1);
  CHECK(tracking.statistics().bytes_in_use == 0);
}

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
