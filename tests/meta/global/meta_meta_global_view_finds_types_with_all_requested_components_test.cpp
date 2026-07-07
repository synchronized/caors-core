// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "meta_test_support.h"

using namespace caors::tests::meta_registry;
TEST_CASE("meta - global view finds types with all requested components", "[meta]") {
  REQUIRE(caors::meta::type_<view_actor_type>("caors.test.meta.view_actor").result() ==
          caors::error{});
  const auto actor = caors::meta::find<view_actor_type>();
  REQUIRE(actor);
  const caors::type_id actor_id = actor.id();

  REQUIRE(caors::meta::type_<view_player_type>("caors.test.meta.view_player")
              .parent(actor_id)
              .result() == caors::error{});
  const auto player = caors::meta::find<view_player_type>();
  REQUIRE(player);
  const caors::type_id player_id = player.id();

  REQUIRE(caors::meta::get_registry().emplace<view_factory_meta>(actor_id, true) == caors::error{});
  REQUIRE(caors::meta::get_registry().emplace<view_factory_meta>(player_id, true) ==
          caors::error{});
  REQUIRE(caors::meta::get_registry().emplace<view_reflection_meta>(player_id, "Player") ==
          caors::error{});

  auto object_view = caors::meta::get_registry().view<view_factory_meta>();
  CHECK(object_view.size() == 2);

  auto reflected_object_view =
      caors::meta::get_registry().view<view_factory_meta, view_reflection_meta>();
  REQUIRE(reflected_object_view.size() == 1);
  CHECK(reflected_object_view.ids().front() == player_id);

  auto reflection = reflected_object_view.find<view_reflection_meta>(player_id);
  REQUIRE(reflection.has_value());
  CHECK(reflection->display_name == "Player");
  CHECK(caors::meta::is_a(player_id, actor_id));
}
