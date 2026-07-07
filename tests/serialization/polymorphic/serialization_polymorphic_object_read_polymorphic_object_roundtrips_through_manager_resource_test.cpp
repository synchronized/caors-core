// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_polymorphic_object_test_support.h"

using namespace caors::tests::serialization::polymorphic_object;

TEST_CASE("serialization - polymorphic object roundtrips through manager resource",
          "[serialization]") {
  require_metadata();

  caors::objects::manager manager;
  caors::serialization::context context{};
  context.objects.manager = &manager;

  player player_value{};
  player_value.level = 9;
  player_value.name = "Mina";

  const caors::serialization::serialization_text_result json =
      caors::serialization::serialize_polymorphic_json(context, player_value);
  REQUIRE(json.succeeded());

  const caors::result<actor*> decoded =
      caors::serialization::deserialize_polymorphic_json<actor>(context, json.value);
  REQUIRE(decoded.succeeded());
  REQUIRE(decoded.value != nullptr);
  CHECK(manager.find(decoded.value->id()) == decoded.value);
  CHECK(decoded.value->type_id() == player::static_type_id());

  auto* decoded_player = dynamic_cast<player*>(decoded.value);
  REQUIRE(decoded_player != nullptr);
  CHECK(decoded_player->level == 9);
  CHECK(decoded_player->name == "Mina");

  manager.clear();
  CHECK(manager.empty());
}
