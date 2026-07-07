// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_object_ref_basic_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::object_ref_basic;
TEST_CASE("serialization - serializes empty object refs as null", "[serialization]") {
  require_object_ref_owner_actor_metadata();

  const object_ref_owner_actor actor{};

  const caors::serialization::serialization_text_result json = caors::serialize_json(actor);

  REQUIRE(json.succeeded());
  CHECK(json.value == R"({"owner":null})");
}
