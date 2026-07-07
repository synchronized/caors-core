// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/core/guid.h"

#include <catch2/catch_all.hpp>

TEST_CASE("core - default guid is nil", "[core]") {
  constexpr caors::guid value = caors::guid::nil();

  CHECK_FALSE(value);
  CHECK(value.is_nil());
  CHECK(value.string() == "00000000-0000-0000-0000-000000000000");
}
