// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta.h"

#include <catch2/catch_all.hpp>

#include <string_view>

namespace {

struct actor_type {};

struct player_type : actor_type {};

struct object_factory_meta {
  bool constructible = false;
};

struct reflection_meta {
  std::string_view display_name{};
};

} // namespace

TEST_CASE("meta - registry stores base type information", "[meta]") {
  caors::meta_registry registry;

  CHECK(caors::register_type<actor_type>(registry, "caors.test.meta.actor") ==
        caors::type_register_result::success);

  auto type = registry.find_type("caors.test.meta.actor");
  REQUIRE(type.has_value());
  CHECK(type->name == "caors.test.meta.actor");
  CHECK(type->size == sizeof(actor_type));
  CHECK(type->alignment == alignof(actor_type));
}

TEST_CASE("meta - registry stores components by type id", "[meta]") {
  caors::meta_registry registry;

  REQUIRE(caors::register_type<actor_type>(registry, "caors.test.meta.actor") ==
          caors::type_register_result::success);
  const auto actor = registry.find_type<actor_type>();
  REQUIRE(actor.has_value());
  const caors::type_id actor_id = actor->id;

  CHECK(registry.emplace<object_factory_meta>(actor_id, true) ==
        caors::meta_component_result::success);
  CHECK(registry.has<object_factory_meta>(actor_id));
  CHECK(registry.count<object_factory_meta>() == 1);

  auto factory = registry.find<object_factory_meta>(actor_id);
  REQUIRE(factory.has_value());
  CHECK(factory->constructible);

  CHECK(registry.emplace<object_factory_meta>(actor_id, false) ==
        caors::meta_component_result::duplicate_component);
  CHECK(registry.remove<object_factory_meta>(actor_id));
  CHECK_FALSE(registry.has<object_factory_meta>(actor_id));
}

TEST_CASE("meta - registry rejects components for missing types", "[meta]") {
  caors::meta_registry registry;

  CHECK(registry.emplace<object_factory_meta>(caors::type_id{999}, true) ==
        caors::meta_component_result::missing_type);
  CHECK(registry.emplace<object_factory_meta>(caors::type_id::invalid(), true) ==
        caors::meta_component_result::invalid_type);
}

TEST_CASE("meta - view finds types with all requested components", "[meta]") {
  caors::meta_registry registry;

  REQUIRE(caors::register_type<actor_type>(registry, "caors.test.meta.actor") ==
          caors::type_register_result::success);
  const auto actor = registry.find_type<actor_type>();
  REQUIRE(actor.has_value());
  const caors::type_id actor_id = actor->id;

  REQUIRE(caors::register_type<player_type>(registry, "caors.test.meta.player", actor_id) ==
          caors::type_register_result::success);
  const auto player = registry.find_type<player_type>();
  REQUIRE(player.has_value());
  const caors::type_id player_id = player->id;

  REQUIRE(registry.emplace<object_factory_meta>(actor_id, true) ==
          caors::meta_component_result::success);
  REQUIRE(registry.emplace<object_factory_meta>(player_id, true) ==
          caors::meta_component_result::success);
  REQUIRE(registry.emplace<reflection_meta>(player_id, "Player") ==
          caors::meta_component_result::success);

  auto object_view = registry.view<object_factory_meta>();
  CHECK(object_view.size() == 2);

  auto reflected_object_view = registry.view<object_factory_meta, reflection_meta>();
  REQUIRE(reflected_object_view.size() == 1);
  CHECK(reflected_object_view.ids().front() == player_id);

  auto reflection = reflected_object_view.find<reflection_meta>(player_id);
  REQUIRE(reflection.has_value());
  CHECK(reflection->display_name == "Player");
  CHECK(registry.is_a(player_id, actor_id));
}
