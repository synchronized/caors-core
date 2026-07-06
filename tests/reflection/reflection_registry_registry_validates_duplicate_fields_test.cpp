// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "reflection_registry_test_support.h"

using namespace test_reflection;

TEST_CASE("reflection - registry validates duplicate fields", "[reflection]") {
  CHECK(caors::reflection::class_<duplicate_field_owner>("caors.test.reflection.duplicate_field")
            .field("id", &duplicate_field_owner::id)
            .field("id", &duplicate_field_owner::health)
            .commit() == reflection_error(caors::errc::duplicate_field));
}
