// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/error.h"
#include "caors/meta.h"
#include "caors/object.h"
#include "caors/reflection.h"
#include "caors/result.h"

#include <catch2/catch_all.hpp>

#include <system_error>

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
