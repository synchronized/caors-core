// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/error.h"
#include "caors/meta.h"
#include "caors/object.h"
#include "caors/meta.h"
#include "caors/result.h"

#include <catch2/catch_all.hpp>

#include <system_error>

TEST_CASE("error - lightweight status keeps domain and code", "[error]") {
  constexpr caors::error ok = caors::error::ok();
  CHECK(ok.succeeded());
  CHECK_FALSE(ok.failed());
  CHECK_FALSE(ok);
  CHECK(ok.domain() == caors::error_domain::none);
  CHECK(ok.value() == caors::errc::success);

  constexpr caors::error failed{caors::error_domain::serialization, caors::errc::unsupported};
  CHECK(failed.failed());
  CHECK_FALSE(failed.succeeded());
  CHECK(failed);
  CHECK(failed.domain() == caors::error_domain::serialization);
  CHECK(failed.value() == caors::errc::unsupported);
  CHECK(failed.code().message() == "unsupported");
}
