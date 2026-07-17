// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#pragma once

#include "serialization_test_support.h"

#include "caors/object.h"

#include <cstdint>
#include <string_view>

namespace caors::tests::serialization::object_ref_polymorphic {

class polymorphic_object_ref_actor : public caors::object_base<polymorphic_object_ref_actor>,
                                     public caors::objects::ref_counted {
public:
  static constexpr std::string_view object_type_name =
      "caors.test.serialization.config.polymorphic_object_ref.actor";
};

class polymorphic_object_ref_player
    : public caors::object_base<polymorphic_object_ref_player, polymorphic_object_ref_actor> {
public:
  static constexpr std::string_view object_type_name =
      "caors.test.serialization.config.polymorphic_object_ref.player";

  ~polymorphic_object_ref_player() override { ++destroyed_count; }

  int level = 0;
  caors::string name{};

  inline static int destroyed_count = 0;
};

class polymorphic_object_ref_enemy : public caors::object_base<polymorphic_object_ref_enemy>,
                                     public caors::objects::ref_counted {
public:
  static constexpr std::string_view object_type_name =
      "caors.test.serialization.config.polymorphic_object_ref.enemy";

  int danger = 0;
};

struct polymorphic_object_ref_holder {
  caors::object_ref<polymorphic_object_ref_actor> owner{};
};

constexpr caors::type_guid polymorphic_object_ref_actor_guid{caors::guid::from_bytes(
    caors::guid::byte_array{0x3e, 0x1a, 0xa8, 0x02, 0x3d, 0x91, 0x4c, 0xa6, 0x8f, 0x31, 0x2b, 0xc5,
                            0xd5, 0x84, 0x10, 0x01})};

constexpr caors::type_guid polymorphic_object_ref_player_guid{caors::guid::from_bytes(
    caors::guid::byte_array{0x3e, 0x1a, 0xa8, 0x02, 0x3d, 0x91, 0x4c, 0xa6, 0x8f, 0x31, 0x2b, 0xc5,
                            0xd5, 0x84, 0x10, 0x02})};

constexpr caors::type_guid polymorphic_object_ref_enemy_guid{caors::guid::from_bytes(
    caors::guid::byte_array{0x3e, 0x1a, 0xa8, 0x02, 0x3d, 0x91, 0x4c, 0xa6, 0x8f, 0x31, 0x2b, 0xc5,
                            0xd5, 0x84, 0x10, 0x03})};

inline void require_polymorphic_object_ref_metadata() {
  REQUIRE(type_result_ok(caors::meta::class_<polymorphic_object_ref_actor>(
                             polymorphic_object_ref_actor::object_type_name)
                             .guid(polymorphic_object_ref_actor_guid)
                             .result()));
  REQUIRE(type_result_ok(caors::meta::class_<polymorphic_object_ref_player>(
                             polymorphic_object_ref_player::object_type_name)
                             .guid(polymorphic_object_ref_player_guid)
                             .parent<polymorphic_object_ref_actor>()
                             .result()));
  REQUIRE(type_result_ok(caors::meta::class_<polymorphic_object_ref_enemy>(
                             polymorphic_object_ref_enemy::object_type_name)
                             .guid(polymorphic_object_ref_enemy_guid)
                             .result()));

  REQUIRE(reflection_result_ok(caors::meta::class_<polymorphic_object_ref_actor>().commit()));
  REQUIRE(reflection_result_ok(caors::meta::class_<polymorphic_object_ref_player>()
                                   .field("level", &polymorphic_object_ref_player::level)
                                   .field("name", &polymorphic_object_ref_player::name)
                                   .commit()));
  REQUIRE(reflection_result_ok(caors::meta::class_<polymorphic_object_ref_enemy>()
                                   .field("danger", &polymorphic_object_ref_enemy::danger)
                                   .commit()));
  REQUIRE(
      reflection_result_ok(((void)caors::meta::class_<polymorphic_object_ref_holder>(
                                "caors.test.serialization.config.polymorphic_object_ref.holder"),
                            caors::meta::class_<polymorphic_object_ref_holder>())
                               .field("owner", &polymorphic_object_ref_holder::owner)
                               .commit()));

  REQUIRE(serialization_result_ok(
      caors::serialization::class_<polymorphic_object_ref_actor>().commit()));
  REQUIRE(serialization_result_ok(
      caors::serialization::class_<polymorphic_object_ref_player>().commit()));
  REQUIRE(serialization_result_ok(
      caors::serialization::class_<polymorphic_object_ref_enemy>().commit()));
  REQUIRE(serialization_result_ok(
      caors::serialization::class_<polymorphic_object_ref_holder>().commit()));
}

} // namespace caors::tests::serialization::object_ref_polymorphic
