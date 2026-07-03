// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta.h"

#include <catch2/catch_all.hpp>

#include <cstdint>
#include <limits>
#include <string_view>

namespace {

struct base_actor_type {};

struct component_actor_type {};

struct missing_component_meta {
  bool constructible = false;
};

struct factory_meta {
  bool constructible = false;
};

struct view_actor_type {};

struct view_player_type : view_actor_type {};

struct view_factory_meta {
  bool constructible = false;
};

struct view_reflection_meta {
  std::string_view display_name{};
};

[[nodiscard]] constexpr caors::error meta_error(caors::errc code) noexcept {
  return {caors::error_domain::meta, code};
}

}

TEST_CASE("meta - global registry stores base type information", "[meta]") {
  CHECK(caors::meta::type_<base_actor_type>("caors.test.meta.base_actor").result() ==
        caors::error{});

  auto type = caors::meta::find("caors.test.meta.base_actor");
  REQUIRE(type);
  CHECK(type.name() == "caors.test.meta.base_actor");
  CHECK(type.size() == sizeof(base_actor_type));
  CHECK(type.alignment() == alignof(base_actor_type));
}

TEST_CASE("meta - global registry stores components by type id", "[meta]") {
  REQUIRE(caors::meta::type_<component_actor_type>("caors.test.meta.component_actor").result() ==
          caors::error{});
  const auto actor = caors::meta::find<component_actor_type>();
  REQUIRE(actor);
  const caors::type_id actor_id = actor.id();

  CHECK(caors::meta::get_registry().emplace<factory_meta>(actor_id, true) == caors::error{});
  CHECK(caors::meta::get_registry().has<factory_meta>(actor_id));
  CHECK(caors::meta::get_registry().count<factory_meta>() == 1);

  auto factory = caors::meta::get_registry().find<factory_meta>(actor_id);
  REQUIRE(factory.has_value());
  CHECK(factory->constructible);

  const factory_meta* factory_ptr = caors::meta::get_registry().find_ptr<factory_meta>(actor_id);
  REQUIRE(factory_ptr != nullptr);
  CHECK(factory_ptr->constructible);

  CHECK(caors::meta::get_registry().emplace<factory_meta>(actor_id, false) ==
        meta_error(caors::errc::duplicate_component));
  CHECK(caors::meta::get_registry().has<factory_meta>(actor_id));
  CHECK(caors::meta::get_registry().component_status<factory_meta>(actor_id) ==
        caors::meta::component_status::present);
  CHECK(caors::meta::get_registry().component_status<view_reflection_meta>(actor_id) ==
        caors::meta::component_status::missing_component);
  CHECK(caors::meta::get_registry().component_status<factory_meta>(caors::type_id::invalid()) ==
        caors::meta::component_status::invalid_type);
  CHECK(caors::meta::get_registry().component_status<factory_meta>(
            caors::type_id{std::numeric_limits<std::uint64_t>::max()}) ==
        caors::meta::component_status::missing_type);
}

TEST_CASE("meta - global registry rejects components for missing types", "[meta]") {
  CHECK(caors::meta::get_registry().emplace<missing_component_meta>(
            caors::type_id{std::numeric_limits<std::uint64_t>::max()}, true) ==
        meta_error(caors::errc::missing_type));
  CHECK(caors::meta::get_registry().emplace<missing_component_meta>(
            caors::type_id::invalid(), true) == meta_error(caors::errc::invalid_type));
}

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
