// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - serializes reflected struct fields to json", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<serializable_actor>("caors.test.serialization.actor")
          .field("id", &serializable_actor::id)
          .field("health", &serializable_actor::health)
          .field("active", &serializable_actor::active)
          .field("name", &serializable_actor::name)
          .commit()));

  const caors::error register_status = caors::serialization::register_type<serializable_actor>();
  REQUIRE(serialization_result_ok(register_status));

  const serializable_actor actor{7, 95.5F, true, "Ada\nLovelace"};
  const caors::serialization::serialization_text_result result = caors::serialize_json(actor);

  REQUIRE(result.succeeded());
  CHECK(result.value == R"({"id":7,"health":95.5,"active":true,"name":"Ada\nLovelace"})");
}
