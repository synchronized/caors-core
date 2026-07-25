// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/platform/shared_library.h"

#include <catch2/catch_all.hpp>

#include <filesystem>
#include <type_traits>

static_assert(!std::is_copy_constructible_v<caors::platform::shared_library>);
static_assert(!std::is_copy_assignable_v<caors::platform::shared_library>);
static_assert(std::is_nothrow_move_constructible_v<caors::platform::shared_library>);
static_assert(std::is_nothrow_move_assignable_v<caors::platform::shared_library>);

TEST_CASE("platform shared library - default and close are empty", "[platform]") {
  caors::platform::shared_library library;

  CHECK_FALSE(library.loaded());
  CHECK(library.path().empty());
  CHECK(library.find_symbol("missing") == nullptr);
  CHECK(library.close().succeeded());
}

TEST_CASE("platform shared library - rejects invalid paths", "[platform]") {
  const auto empty = caors::platform::shared_library::open({});
  REQUIRE_FALSE(empty);
  CHECK(empty.error() ==
        caors::error{caors::error_domain::platform, caors::errc::invalid_argument});

  const std::filesystem::path missing =
      std::filesystem::current_path() / "caors_missing_shared_library_for_test";
  const auto absent = caors::platform::shared_library::open(missing);
  REQUIRE_FALSE(absent);
  CHECK(absent.error() == caors::error{caors::error_domain::platform, caors::errc::not_found});
}
