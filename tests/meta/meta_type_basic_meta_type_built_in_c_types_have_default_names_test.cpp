// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "meta_type_basic_test_support.h"

using namespace caors::tests::meta_type_basic;
TEST_CASE("meta type - built-in C++ types have default names", "[meta]") {
  CHECK(caors::traits::type_name<int>::value == "int");
  CHECK(caors::traits::type_name<float>::value == "float");
  CHECK(caors::traits::type_name<bool>::value == "bool");
}
