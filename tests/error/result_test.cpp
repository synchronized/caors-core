// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/error.h"
#include "caors/meta.h"
#include "caors/object.h"
#include "caors/reflection.h"
#include "caors/result.h"

#include <catch2/catch_all.hpp>

#include <system_error>

TEST_CASE("result - carries value or error status", "[error]") {
  caors::result<int> ok{{}, 42};
  CHECK(ok);
  CHECK(ok.succeeded());
  CHECK_FALSE(ok.failed());
  CHECK_FALSE(ok.status);
  CHECK(ok.value == 42);

  caors::result<int> failed{{caors::error_domain::core, caors::errc::invalid_argument}, {}};
  CHECK_FALSE(failed);
  CHECK_FALSE(failed.succeeded());
  CHECK(failed.failed());
  CHECK(failed.status);
  CHECK(failed.status.domain() == caors::error_domain::core);
  CHECK(failed.status.value() == caors::errc::invalid_argument);
}
