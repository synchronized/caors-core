// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "reflection_registry_test_support.h"

using namespace test_reflection;

TEST_CASE("reflection - views remain valid for append-only registry", "[reflection]") {
  const caors::error register_status = register_view_actor();
  REQUIRE((register_status == caors::error{} ||
           register_status == reflection_error(caors::errc::duplicate_reflection)));

  auto reflected = caors::reflection::find("caors.test.reflection.view_actor");
  REQUIRE(reflected);

  auto field = reflected.find_field("id");
  REQUIRE(field);
  CHECK(reflected.valid());
  CHECK(field.valid());

  CHECK(register_view_actor() == reflection_error(caors::errc::duplicate_reflection));

  CHECK(reflected.valid());
  CHECK(field.valid());
}
