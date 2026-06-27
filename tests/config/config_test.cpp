// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/config.h"

#include <catch2/catch_all.hpp>

#include <cstddef>

namespace {

bool is_power_of_two(std::size_t value) noexcept {
  return value != 0 && (value & (value - 1)) == 0;
}

}

TEST_CASE("config - default new alignment", "[config]") {
  CHECK(caors::config::default_new_alignment >= alignof(std::max_align_t));
  CHECK(is_power_of_two(caors::config::default_new_alignment));
}

TEST_CASE("config - exception detection matches macro", "[config]") {
  CHECK(caors::config::has_exceptions == (CAORS_HAS_EXCEPTIONS != 0));
}
