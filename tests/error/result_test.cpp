// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/error.h"
#include "caors/meta.h"
#include "caors/object.h"
#include "caors/reflection.h"
#include "caors/result.h"

#include <catch2/catch_all.hpp>

#include <string>
#include <system_error>

TEST_CASE("result - carries value or error status", "[error]") {
  caors::result<int> ok{{}, 42};
  CHECK(ok);
  CHECK(ok.succeeded());
  CHECK_FALSE(ok.failed());
  CHECK(ok.has_value());
  CHECK_FALSE(ok.error());
  CHECK(*ok == 42);
  CHECK(ok.value_or(7) == 42);

  caors::result<int> failed{{caors::error_domain::core, caors::errc::invalid_argument}, {}};
  CHECK_FALSE(failed);
  CHECK_FALSE(failed.succeeded());
  CHECK(failed.failed());
  CHECK_FALSE(failed.has_value());
  CHECK(failed.error());
  CHECK(failed.error().domain() == caors::error_domain::core);
  CHECK(failed.error().value() == caors::errc::invalid_argument);
  CHECK(failed.value_or(7) == 7);
}

TEST_CASE("result - provides pointer-like value access", "[error]") {
  caors::result<std::string> ok{{}, "caors"};

  CHECK(ok->size() == 5);
  CHECK((*ok) == "caors");

  std::string moved = *std::move(ok);
  CHECK(moved == "caors");
}
