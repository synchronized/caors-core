// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "config_test_support.h"

using namespace caors::tests::config;
TEST_CASE("config - exception detection matches macro", "[config]") {
  CHECK(caors::config::has_exceptions == (CAORS_HAS_EXCEPTIONS != 0));
}
