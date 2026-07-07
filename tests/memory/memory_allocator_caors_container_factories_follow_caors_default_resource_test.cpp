// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/core/containers.h"
#include "caors/memory.h"

#include <catch2/catch_all.hpp>

#include <string_view>

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
