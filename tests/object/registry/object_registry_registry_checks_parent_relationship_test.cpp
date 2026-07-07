// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "object_test_common.h"

using namespace caors::tests::object;

TEST_CASE("object - registry checks parent relationship", "[object]") {
  REQUIRE(register_object_test_type<relation_actor>() == caors::error{});
  const auto object_type = caors::objects::find<caors::object>();
  const auto actor_type = caors::objects::find<relation_actor>();
  REQUIRE(object_type);
  REQUIRE(actor_type);

  REQUIRE(register_object_test_type<relation_player>(actor_type.id()) == caors::error{});
  const auto player_type = caors::objects::find<relation_player>();
  REQUIRE(player_type);

  CHECK(caors::objects::is_a(player_type.id(), player_type.id()));
  CHECK(caors::objects::is_a(player_type.id(), actor_type.id()));
  CHECK(caors::objects::is_a(player_type.id(), object_type.id()));
  CHECK_FALSE(caors::objects::is_a(actor_type.id(), player_type.id()));
}
