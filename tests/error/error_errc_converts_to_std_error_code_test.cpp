// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/error.h"
#include "caors/meta.h"
#include "caors/object.h"
#include "caors/reflection.h"
#include "caors/result.h"

#include <catch2/catch_all.hpp>

#include <system_error>

TEST_CASE("error - errc converts to std error code", "[error]") {
  const std::error_code code = caors::errc::invalid_type;

  CHECK(code);
  CHECK(code.value() == static_cast<int>(caors::errc::invalid_type));
  CHECK(code.category() == caors::error_category());
  CHECK(code.message() == "invalid type");

  const std::error_code frozen = caors::errc::frozen;
  CHECK(frozen);
  CHECK(frozen.message() == "frozen");

  const std::error_code duplicate_method = caors::errc::duplicate_method;
  CHECK(duplicate_method);
  CHECK(duplicate_method.message() == "duplicate method");
}
