// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "meta_test_support.h"

using namespace caors::tests::meta_registry;
TEST_CASE("meta - global registry stores base type information", "[meta]") {
  CHECK(caors::meta::type_<base_actor_type>("caors.test.meta.base_actor").result() ==
        caors::error{});

  auto type = caors::meta::find("caors.test.meta.base_actor");
  REQUIRE(type);
  CHECK(type.name() == "caors.test.meta.base_actor");
  CHECK(type.size() == sizeof(base_actor_type));
  CHECK(type.alignment() == alignof(base_actor_type));
}
