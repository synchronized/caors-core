// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#pragma once

#include "caors/config.h"

#include <catch2/catch_all.hpp>

#include <cstddef>

namespace caors::tests::config {

inline bool is_power_of_two(std::size_t value) noexcept {
  return value != 0 && (value & (value - 1)) == 0;
}

} // namespace caors::tests::config
