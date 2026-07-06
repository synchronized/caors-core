// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#pragma once

#include "serialization_test_support.h"

#include <memory>
#include <string_view>

namespace caors::tests::serialization::polymorphic_value_wrapper {

class polymorphic_value_actor {
public:
  virtual ~polymorphic_value_actor() = default;
};

class polymorphic_value_player : public polymorphic_value_actor {
public:
  int level = 0;
  caors::string name{};
};

class polymorphic_value_enemy {
public:
  int danger = 0;
};

struct polymorphic_value_holder {
  std::unique_ptr<polymorphic_value_actor> owner{};
  std::shared_ptr<polymorphic_value_actor> shared{};
};

constexpr caors::type_guid actor_guid{caors::guid::from_bytes(
    caors::guid::byte_array{0x0f, 0xc2, 0x7d, 0x83, 0x34, 0x4a, 0x42, 0x58, 0x90, 0x9d, 0xf9, 0x0e,
                            0x5c, 0xb7, 0x20, 0x01})};

constexpr caors::type_guid player_guid{caors::guid::from_bytes(
    caors::guid::byte_array{0x0f, 0xc2, 0x7d, 0x83, 0x34, 0x4a, 0x42, 0x58, 0x90, 0x9d, 0xf9, 0x0e,
                            0x5c, 0xb7, 0x20, 0x02})};

constexpr caors::type_guid enemy_guid{caors::guid::from_bytes(
    caors::guid::byte_array{0x0f, 0xc2, 0x7d, 0x83, 0x34, 0x4a, 0x42, 0x58, 0x90, 0x9d, 0xf9, 0x0e,
                            0x5c, 0xb7, 0x20, 0x03})};

[[nodiscard]] inline bool type_result_ok(caors::error result) noexcept {
  return result.succeeded() || result.value() == caors::errc::duplicate_id;
}

inline void require_polymorphic_value_metadata() {
  REQUIRE(type_result_ok(
      caors::meta::type_<polymorphic_value_actor>("caors.test.serialization.poly_value.actor")
          .guid(actor_guid)
          .result()));
  REQUIRE(type_result_ok(
      caors::meta::type_<polymorphic_value_player>("caors.test.serialization.poly_value.player")
          .guid(player_guid)
          .parent<polymorphic_value_actor>()
          .result()));
  REQUIRE(type_result_ok(
      caors::meta::type_<polymorphic_value_enemy>("caors.test.serialization.poly_value.enemy")
          .guid(enemy_guid)
          .result()));
  REQUIRE(type_result_ok(caors::meta::type_<std::unique_ptr<polymorphic_value_actor>>(
                             "std.unique_ptr<caors.test.serialization.poly_value.actor>")
                             .result()));
  REQUIRE(type_result_ok(caors::meta::type_<std::shared_ptr<polymorphic_value_actor>>(
                             "std.shared_ptr<caors.test.serialization.poly_value.actor>")
                             .result()));

  REQUIRE(caors::tests::serialization::reflection_result_ok(
      caors::reflection::class_<polymorphic_value_actor>(
          caors::meta::find<polymorphic_value_actor>())
          .commit()));
  REQUIRE(caors::tests::serialization::reflection_result_ok(
      caors::reflection::class_<polymorphic_value_player>(
          caors::meta::find<polymorphic_value_player>())
          .field("level", &polymorphic_value_player::level)
          .field("name", &polymorphic_value_player::name)
          .commit()));
  REQUIRE(caors::tests::serialization::reflection_result_ok(
      caors::reflection::class_<polymorphic_value_enemy>(
          caors::meta::find<polymorphic_value_enemy>())
          .field("danger", &polymorphic_value_enemy::danger)
          .commit()));
  REQUIRE(caors::tests::serialization::reflection_result_ok(
      caors::reflection::class_<polymorphic_value_holder>(
          "caors.test.serialization.poly_value.holder")
          .field("owner", &polymorphic_value_holder::owner,
                 "std.unique_ptr<caors.test.serialization.poly_value.actor>")
          .field("shared", &polymorphic_value_holder::shared,
                 "std.shared_ptr<caors.test.serialization.poly_value.actor>")
          .commit()));

  REQUIRE(caors::tests::serialization::serialization_result_ok(
      caors::serialization::register_type<polymorphic_value_actor>()));
  REQUIRE(caors::tests::serialization::serialization_result_ok(
      caors::serialization::register_type<polymorphic_value_player>()));
  REQUIRE(caors::tests::serialization::serialization_result_ok(
      caors::serialization::register_type<polymorphic_value_enemy>()));
  REQUIRE(caors::tests::serialization::serialization_result_ok(
      caors::serialization::register_type<polymorphic_value_holder>()));
}

} // namespace caors::tests::serialization::polymorphic_value_wrapper
