// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "config_test_support.h"

using namespace caors::tests::config;
TEST_CASE("config - default new alignment", "[config]") {
  CHECK(caors::config::default_new_alignment >= alignof(std::max_align_t));
  CHECK(is_power_of_two(caors::config::default_new_alignment));
}
