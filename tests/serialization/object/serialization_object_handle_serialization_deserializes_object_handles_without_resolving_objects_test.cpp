// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_object_handle_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::object_handle;
TEST_CASE("serialization - deserializes object handles without resolving objects",
          "[serialization]") {
  require_object_handle_actor_metadata();

  object_handle_actor actor{};
  const caors::error result = caors::deserialize_json(
      actor, R"({"target":{"id":77,"type":99},"typed":{"id":88,"type":111}})");

  REQUIRE(result.succeeded());
  CHECK(actor.target == caors::object_handle{caors::object_id{77}, caors::object_type_id{99}});
  CHECK(actor.typed.handle() ==
        caors::object_handle{caors::object_id{88}, caors::object_type_id{111}});
}
