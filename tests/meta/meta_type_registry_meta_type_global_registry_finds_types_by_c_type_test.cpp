// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "meta_type_registry_test_support.h"

using namespace caors::tests::meta_type_registry;
TEST_CASE("meta type - global registry finds types by C++ type", "[meta]") {
  REQUIRE(caors::meta::type_<cpp_sample_type>("caors.test.type.cpp_sample").result() ==
          caors::error{});

  auto type = caors::meta::find<cpp_sample_type>();
  REQUIRE(type);
  CHECK(type.name() == "caors.test.type.cpp_sample");
  CHECK(caors::meta::id("caors.test.type.cpp_sample") == type.id());
}
