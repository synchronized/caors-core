// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "reflection_registry_test_support.h"

using namespace test_reflection;

TEST_CASE("reflection - registry reports duplicate reflection", "[reflection]") {
  REQUIRE(caors::reflection::class_<duplicate_actor>("caors.test.reflection.duplicate_actor")
              .field("id", &duplicate_actor::id)
              .commit() == caors::error{});
  CHECK(caors::reflection::class_<duplicate_actor>("caors.test.reflection.duplicate_actor")
            .field("id", &duplicate_actor::id)
            .commit() == reflection_error(caors::errc::duplicate_reflection));
}
