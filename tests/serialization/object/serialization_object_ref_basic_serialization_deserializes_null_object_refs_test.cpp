// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_object_ref_basic_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::object_ref_basic;
TEST_CASE("serialization - deserializes null object refs", "[serialization]") {
  require_object_ref_owner_actor_metadata();

  object_ref_owner_actor actor{};
  actor.owner = caors::objects::make_ref<object_ref_target>();
  actor.owner->value = 9;

  const caors::error result = caors::deserialize_json(actor, R"({"owner":null})");

  REQUIRE(result.succeeded());
  CHECK(actor.owner == nullptr);
}
