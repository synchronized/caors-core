// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_polymorphic_object_test_support.h"

using namespace caors::tests::serialization::polymorphic_object;

TEST_CASE("serialization - ordinary object API reads runtime type envelopes", "[serialization]") {
  require_metadata();

  player source{};
  source.level = 11;
  source.name = "Ren";
  const actor& source_actor = source;

  const caors::serialization::serialization_text_result json =
      caors::serialize_json(source_actor);
  REQUIRE(json.succeeded());

  player target{};
  actor& target_actor = target;
  const caors::error status = caors::deserialize_json(target_actor, json.value);
  REQUIRE(status.succeeded());
  CHECK(target.level == 11);
  CHECK(target.name == "Ren");

  actor wrong_target{};
  const caors::error wrong_status = caors::deserialize_json(wrong_target, json.value);
  CHECK(wrong_status.failed());
  CHECK(wrong_status.value() == caors::errc::invalid_type);
}

TEST_CASE("serialization - polymorphic object roundtrips through manager resource",
          "[serialization]") {
  require_metadata();

  caors::objects::manager manager;
  caors::serialization::context context{};
  context.object_manager = &manager;

  player player_value{};
  player_value.level = 9;
  player_value.name = "Mina";

  const caors::serialization::serialization_text_result json =
      caors::serialization::serialize_polymorphic_json(player_value, context);
  REQUIRE(json.succeeded());

  const caors::result<actor*> decoded =
      caors::serialization::deserialize_polymorphic_json<actor>(json.value, context);
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
