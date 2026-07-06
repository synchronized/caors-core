// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - serializes json to caller provided buffer", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<serializable_actor>("caors.test.serialization.actor")
          .field("id", &serializable_actor::id)
          .field("health", &serializable_actor::health)
          .field("active", &serializable_actor::active)
          .field("name", &serializable_actor::name)
          .commit()));

  const caors::error register_status = caors::serialization::register_type<serializable_actor>();
  REQUIRE(serialization_result_ok(register_status));

  const serializable_actor actor{3, 10.5F, false, "Lin"};
  caors::string output = "previous";
  const caors::error status = caors::serialize_json_to(actor, output);

  REQUIRE(status.succeeded());
  CHECK(output == R"({"id":3,"health":10.5,"active":false,"name":"Lin"})");

  output = "previous";
  const caors::error text_status = caors::serialize_text_to(actor, output);
  REQUIRE(text_status.succeeded());
  CHECK(output == R"({"id":3,"health":10.5,"active":false,"name":"Lin"})");
}
