// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "reflection_test_support.h"

using namespace test_reflection;

TEST_CASE("reflection - field view rejects writes to readonly fields", "[reflection]") {
  REQUIRE(caors::reflection::class_<readonly_actor>("caors.test.reflection.readonly_actor")
              .field("id", &readonly_actor::id)
              .commit() == caors::error{});

  const caors::reflection::field id =
      require_field(require_reflected("caors.test.reflection.readonly_actor"), "id");

  readonly_actor instance{};
  CHECK_FALSE(id.writable());

  auto id_value = id.try_get<int>(&instance);
  REQUIRE(id_value.has_value());
  CHECK(*id_value == 7);
  CHECK_FALSE(id.try_set<int>(&instance, 8));
}
