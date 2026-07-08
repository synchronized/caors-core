// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/core/guid.h"

#include <catch2/catch_all.hpp>

TEST_CASE("core - guid rejects invalid text", "[core]") {
  CHECK_FALSE(caors::guid::parse("").has_value());
  CHECK_FALSE(caors::guid::parse("2f6d1f64-7a3a-4db2-9a2d-123456789ab").has_value());
  CHECK_FALSE(caors::guid::parse("2f6d1f64_7a3a_4db2_9a2d_123456789abc").has_value());
  CHECK_FALSE(caors::guid::parse("2f6d1f64-7a3a-4db2-9a2d-123456789abx").has_value());
}
