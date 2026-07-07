// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_object_handle_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::object_handle;
TEST_CASE("serialization - object manager resolves deserialized typed handles", "[serialization]") {
  require_object_handle_target_metadata();
  require_object_handle_actor_metadata();

  caors::objects::manager manager;
  object_ptr_target* target = manager.create<object_ptr_target>();
  REQUIRE(target != nullptr);

  object_handle_actor actor{};
  actor.target = manager.handle_of(target->id());
  actor.typed = manager.handle_of<object_ptr_target>(target->id());
  REQUIRE(actor.target);
  REQUIRE(actor.typed);

  const caors::serialization::serialization_text_result json = caors::serialize_json(actor);
  REQUIRE(json.succeeded());

  object_handle_actor decoded{};
  const caors::error result = caors::deserialize_json(decoded, json.value);
  REQUIRE(result.succeeded());

  CHECK(decoded.target == actor.target);
  CHECK(decoded.typed == actor.typed);
  CHECK(manager.find(decoded.target) == target);
  CHECK(manager.find(decoded.typed) == target);

  REQUIRE(manager.destroy(target));
  CHECK(manager.find(decoded.target) == nullptr);
  CHECK(manager.find(decoded.typed) == nullptr);
}
