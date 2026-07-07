// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - field metadata renames and skips fields", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<configured_actor>("caors.test.serialization.configured_actor")
          .field("id", &configured_actor::id)
          .field("health", &configured_actor::health)
          .field("debug", &configured_actor::debug)
          .commit()));

  REQUIRE(caors::serialization::register_type<configured_actor>(
              {
                  caors::serialization::make_field("health", "hp"),
                  caors::serialization::make_field("debug", {}, true),
                  caors::serialization::make_field("id"),
              })
              .succeeded());

  const configured_actor actor{7, 42.5F, true};
  const caors::serialization::serialization_text_result result = caors::serialize_text(actor);

  REQUIRE(result.succeeded());
  CHECK(result.value == R"({"hp":42.5,"id":7})");
}
