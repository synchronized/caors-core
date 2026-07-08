// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：string_name_string_name_can_create_caors_string_with_resource_test.cpp

#include "caors/core/string_name.h"
#include "caors/memory/tracking.h"

#include <catch2/catch_all.hpp>

TEST_CASE("core - string name can create caors string with resource", "[core]") {
  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  const caors::string_name name{"caors.test.resource.string"};

  caors::string text = name.string(&tracking);

  CHECK(text == "caors.test.resource.string");
  CHECK(text.get_allocator().resource() == &tracking);
}
