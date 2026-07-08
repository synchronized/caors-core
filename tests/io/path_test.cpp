// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "io_test_support.h"

using namespace caors::tests::io;

TEST_CASE("io - path owns and normalizes virtual paths", "[io]") {
  caors::path owned{"assets\\textures"};
  caors::path_view view{owned};

  CHECK(owned.string_view() == "assets\\textures");
  CHECK(view.string_view() == "assets\\textures");

  caors::path normalized = caors::normalize_path("\\assets//textures\\albedo.png");
  CHECK(normalized.string_view() == "assets/textures/albedo.png");

  caors::path joined = caors::join_path("/assets/textures/", "\\albedo.png");
  CHECK(joined.string_view() == "assets/textures/albedo.png");
}
