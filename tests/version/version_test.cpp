// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/version.h"

#include <catch2/catch_all.hpp>

TEST_CASE("version - get_version()", "[version]") {
  const auto version = caors::get_version();

  CHECK(version == caors::k_version);
  CHECK(caors::get_version_string() == caors::k_version_string);
  CHECK_FALSE(caors::get_version_string().empty());
}
