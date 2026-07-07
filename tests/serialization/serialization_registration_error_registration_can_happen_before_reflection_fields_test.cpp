// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - registration can happen before reflection fields", "[serialization]") {
  const caors::meta::type type = caors::meta::type_<serialization_first_actor>(
                                     "caors.test.serialization.serialization_first_actor")
                                     .get();
  REQUIRE(type.valid());

  REQUIRE(caors::serialization::register_type<serialization_first_actor>(
              {
                  caors::serialization::make_field("id", "actor_id"),
              })
              .succeeded());

  REQUIRE(reflection_result_ok(caors::reflection::class_<serialization_first_actor>(
                                   "caors.test.serialization.serialization_first_actor")
                                   .field("id", &serialization_first_actor::id)
                                   .commit()));

  const serialization_first_actor actor{9};
  const caors::serialization::serialization_text_result result = caors::serialize_text(actor);

  REQUIRE(result.succeeded());
  CHECK(result.value == R"({"actor_id":9})");
}
