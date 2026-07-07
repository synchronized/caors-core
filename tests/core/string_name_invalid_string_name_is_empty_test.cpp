// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/core/string_name.h"
#include "caors/memory/tracking.h"

#include <catch2/catch_all.hpp>

TEST_CASE("core - invalid string name is empty", "[core]") {
  constexpr caors::string_name invalid = caors::string_name::invalid();

  CHECK_FALSE(invalid);
  CHECK(invalid.empty());
  CHECK(invalid.view().empty());
  CHECK(invalid.to_string().empty());
}
