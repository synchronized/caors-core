// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "meta_test_support.h"

using namespace caors::tests::meta_registry;

TEST_CASE("meta detail - registry rejects extensions for missing types", "[meta][detail]") {
  CHECK(caors::meta::detail::emplace_extension<missing_extension_meta>(
            caors::meta::get_registry(), caors::type_id{std::numeric_limits<std::uint64_t>::max()},
            true) == meta_error(caors::errc::missing_type));
  CHECK(caors::meta::detail::emplace_extension<missing_extension_meta>(
            caors::meta::get_registry(), caors::type_id::invalid(), true) ==
        meta_error(caors::errc::invalid_type));
}

TEST_CASE("meta detail - registry stores extensions by type id", "[meta][detail]") {
  REQUIRE(caors::registration::type_<extension_actor_type>("caors.test.meta.extension_actor")
              .commit() == caors::error{});
  const auto actor = caors::meta::find<extension_actor_type>();
  REQUIRE(actor);
  const caors::type_id actor_id = actor.id();

  CHECK(caors::meta::detail::emplace_extension<factory_meta>(caors::meta::get_registry(), actor_id,
                                                             true) == caors::error{});
  CHECK(caors::meta::get_registry().has_extension<factory_meta>(actor_id));
  CHECK(caors::meta::get_registry().extension_count<factory_meta>() == 1);

  auto factory = caors::meta::get_registry().find_extension<factory_meta>(actor_id);
  REQUIRE(factory.has_value());
  CHECK(factory->constructible);

  const factory_meta* factory_ptr =
      caors::meta::get_registry().find_extension_ptr<factory_meta>(actor_id);
  REQUIRE(factory_ptr != nullptr);
  CHECK(factory_ptr->constructible);

  CHECK(caors::meta::detail::emplace_extension<factory_meta>(caors::meta::get_registry(), actor_id,
                                                             false) ==
        meta_error(caors::errc::duplicate_component));
  CHECK(caors::meta::get_registry().has_extension<factory_meta>(actor_id));
  CHECK_FALSE(caors::meta::get_registry().has_extension<view_label_meta>(actor_id));
}

TEST_CASE("meta detail - global view finds types with all requested extensions", "[meta][detail]") {
  REQUIRE(caors::registration::type_<view_actor_type>("caors.test.meta.view_actor").commit() ==
          caors::error{});
  const auto actor = caors::meta::find<view_actor_type>();
  REQUIRE(actor);
  const caors::type_id actor_id = actor.id();

  REQUIRE(caors::registration::type_<view_player_type>("caors.test.meta.view_player")
              .parent(actor_id)
              .commit() == caors::error{});
  const auto player = caors::meta::find<view_player_type>();
  REQUIRE(player);
  const caors::type_id player_id = player.id();

  REQUIRE(caors::meta::detail::emplace_extension<view_factory_meta>(
              caors::meta::get_registry(), actor_id, true) == caors::error{});
  REQUIRE(caors::meta::detail::emplace_extension<view_factory_meta>(
              caors::meta::get_registry(), player_id, true) == caors::error{});
  REQUIRE(caors::meta::detail::emplace_extension<view_label_meta>(
              caors::meta::get_registry(), player_id, "Player") == caors::error{});

  auto object_view = caors::meta::get_registry().extension_view<view_factory_meta>();
  CHECK(object_view.size() == 2);

  auto reflected_object_view =
      caors::meta::get_registry().extension_view<view_factory_meta, view_label_meta>();
  REQUIRE(reflected_object_view.size() == 1);
  CHECK(reflected_object_view.ids().front() == player_id);

  auto label = reflected_object_view.find<view_label_meta>(player_id);
  REQUIRE(label.has_value());
  CHECK(label->display_name == "Player");
  CHECK(caors::meta::is_a(player_id, actor_id));
}
