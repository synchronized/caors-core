// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "reflection_registry_test_support.h"

using namespace test_reflection;

TEST_CASE("reflection - class builder can attach to explicit meta type", "[reflection]") {
  const caors::meta::type type =
      caors::meta::type_<explicit_meta_actor>("caors.test.reflection.explicit_meta_actor").get();
  REQUIRE(type.valid());
  CHECK(caors::meta::find<explicit_meta_actor>().id() == type.id());

  const caors::error status = caors::reflection::class_<explicit_meta_actor>(type)
                                  .field("id", &explicit_meta_actor::id)
                                  .commit();

  REQUIRE(
      (status == caors::error{} || status == reflection_error(caors::errc::duplicate_reflection)));

  auto reflected = caors::reflection::find(type.id());
  REQUIRE(reflected);
  CHECK(reflected.field_count() == 1);
  CHECK(reflected.find_field("id"));
}
