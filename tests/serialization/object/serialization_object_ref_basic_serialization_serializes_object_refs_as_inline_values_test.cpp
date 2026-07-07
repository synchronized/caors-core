// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_object_ref_basic_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::object_ref_basic;
TEST_CASE("serialization - serializes object refs as inline values", "[serialization]") {
  require_object_ref_owner_actor_metadata();

  object_ref_owner_actor actor{};
  actor.owner = caors::objects::make_ref<object_ref_target>();
  actor.owner->value = 42;

  const caors::serialization::serialization_text_result json = caors::serialize_json(actor);

  REQUIRE(json.succeeded());
  CHECK(json.value == R"({"owner":{"value":42}})");
}
