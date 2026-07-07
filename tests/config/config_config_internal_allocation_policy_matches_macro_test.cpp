// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "config_test_support.h"

using namespace caors::tests::config;
TEST_CASE("config - internal allocation policy matches macro", "[config]") {
  CHECK(caors::config::internal_allocations_use_default_resource ==
        (CAORS_INTERNAL_ALLOCATIONS_USE_DEFAULT_RESOURCE != 0));
}
