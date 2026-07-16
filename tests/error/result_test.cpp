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
  caors::result<int> ok = caors::ok(42);
  CHECK(ok);
  CHECK(ok.succeeded());
  CHECK_FALSE(ok.failed());
  CHECK(ok.has_value());
  CHECK_FALSE(ok.error());
  CHECK(*ok == 42);
  CHECK(ok.value_or(7) == 42);

  caors::result<int> failed =
      caors::fail<int>(caors::error{caors::error_domain::core, caors::errc::invalid_argument});
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
  caors::result<std::string> ok = caors::ok(std::string{"caors"});

  CHECK(ok->size() == 5);
  CHECK((*ok) == "caors");

  std::string moved = *std::move(ok);
  CHECK(moved == "caors");
}

TEST_CASE("result - supports void result helpers", "[error]") {
  caors::result<void> ok = caors::ok();
  CHECK(ok);
  CHECK(ok.has_value());
  CHECK_FALSE(ok.error());

  caors::result<void> failed =
      caors::fail(caors::error{caors::error_domain::core, caors::errc::unsupported});
  CHECK_FALSE(failed);
  CHECK_FALSE(failed.has_value());
  CHECK(failed.error());
  CHECK(failed.error().domain() == caors::error_domain::core);
  CHECK(failed.error().value() == caors::errc::unsupported);
}
