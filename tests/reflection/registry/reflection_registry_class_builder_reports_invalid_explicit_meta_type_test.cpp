// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "reflection_registry_test_support.h"

using namespace test_reflection;

TEST_CASE("reflection - class builder reports invalid explicit meta type", "[reflection]") {
  const caors::error status = caors::reflection::class_<explicit_meta_actor>(caors::meta::type{})
                                  .field("id", &explicit_meta_actor::id)
                                  .commit();

  CHECK(status.failed());
  CHECK(status.domain() == caors::error_domain::reflection);
  CHECK(status.value() == caors::errc::invalid_type);
}
