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
}

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

TEST_CASE("error - rethrow_as replaces domain and optional code", "[error]") {
  constexpr caors::error ok = caors::rethrow_as(caors::error{}, caors::error_domain::reflection);
  CHECK(ok.succeeded());
  CHECK_FALSE(ok);
  CHECK(ok.domain() == caors::error_domain::none);

  constexpr caors::error duplicate_component{caors::error_domain::meta,
                                             caors::errc::duplicate_component};
  constexpr caors::error rethrown =
      caors::rethrow_as(duplicate_component, caors::error_domain::serialization);
  CHECK(rethrown.domain() == caors::error_domain::serialization);
  CHECK(rethrown.value() == caors::errc::duplicate_component);

  constexpr caors::error remapped =
      caors::rethrow_as(duplicate_component, caors::error_domain::reflection,
                        caors::errc::duplicate_component, caors::errc::duplicate_reflection);
  CHECK(remapped.domain() == caors::error_domain::reflection);
  CHECK(remapped.value() == caors::errc::duplicate_reflection);
}

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
