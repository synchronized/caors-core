// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/core/string_name.h"

#include <catch2/catch_all.hpp>

TEST_CASE("core - string names are interned", "[core]") {
  const caors::string_name first{"caors.test.sample"};
  const caors::string_name second{"caors.test.sample"};
  const caors::string_name other{"caors.test.other"};

  CHECK(first);
  CHECK(first == second);
  CHECK(first != other);
  CHECK(first.id() == second.id());
  CHECK(first.view() == "caors.test.sample");
  CHECK(first.string() == "caors.test.sample");

  auto found = caors::string_name::find("caors.test.sample");
  REQUIRE(found.has_value());
  CHECK(*found == first);
  CHECK_FALSE(caors::string_name::find("caors.test.not_interned").has_value());
}

TEST_CASE("core - invalid string name is empty", "[core]") {
  constexpr caors::string_name invalid = caors::string_name::invalid();

  CHECK_FALSE(invalid);
  CHECK(invalid.empty());
  CHECK(invalid.id() == 0);
  CHECK(invalid.view().empty());
  CHECK(invalid.string().empty());
}
