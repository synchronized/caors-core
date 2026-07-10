// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#pragma once

#include "serialization_test_support.h"

#include <string_view>

namespace caors::tests::serialization::polymorphic_object {

class actor : public caors::object_base<actor> {
public:
  static constexpr std::string_view object_type_name = "caors.test.serialization.poly.actor";
  static constexpr caors::type_guid object_type_guid{caors::guid::from_bytes(
      caors::guid::byte_array{0x65, 0xa4, 0x1f, 0x25, 0xf8, 0xe1, 0x40, 0xd7, 0x8a, 0x89, 0xf2,
                              0x37, 0x8d, 0xe5, 0x10, 0x01})};
};

class player : public caors::object_base<player, actor> {
public:
  static constexpr std::string_view object_type_name = "caors.test.serialization.poly.player";
  static constexpr caors::type_guid object_type_guid{caors::guid::from_bytes(
      caors::guid::byte_array{0x65, 0xa4, 0x1f, 0x25, 0xf8, 0xe1, 0x40, 0xd7, 0x8a, 0x89, 0xf2,
                              0x37, 0x8d, 0xe5, 0x10, 0x02})};

  int level = 0;
  caors::string name{};
};

class enemy : public caors::object_base<enemy> {
public:
  static constexpr std::string_view object_type_name = "caors.test.serialization.poly.enemy";
  static constexpr caors::type_guid object_type_guid{caors::guid::from_bytes(
      caors::guid::byte_array{0x65, 0xa4, 0x1f, 0x25, 0xf8, 0xe1, 0x40, 0xd7, 0x8a, 0x89, 0xf2,
                              0x37, 0x8d, 0xe5, 0x10, 0x03})};

  int danger = 0;
};

[[nodiscard]] inline bool object_result_ok(caors::error result) noexcept {
  return result.succeeded() || result.value() == caors::errc::duplicate_id;
}

inline void require_metadata() {
  REQUIRE(object_result_ok(caors::objects::class_<actor>().commit()));
  REQUIRE(object_result_ok(caors::objects::class_<player>().commit()));
  REQUIRE(object_result_ok(caors::objects::class_<enemy>().commit()));

  REQUIRE(caors::tests::serialization::reflection_result_ok(
      caors::reflection::class_<actor>(caors::meta::find<actor>()).commit()));
  REQUIRE(caors::tests::serialization::reflection_result_ok(
      caors::reflection::class_<player>(caors::meta::find<player>())
          .field("level", &player::level)
          .field("name", &player::name)
          .commit()));
  REQUIRE(caors::tests::serialization::reflection_result_ok(
      caors::reflection::class_<enemy>(caors::meta::find<enemy>())
          .field("danger", &enemy::danger)
          .commit()));

  REQUIRE(caors::tests::serialization::serialization_result_ok(
      caors::serialization::class_<actor>().commit()));
  REQUIRE(caors::tests::serialization::serialization_result_ok(
      caors::serialization::class_<player>().commit()));
  REQUIRE(caors::tests::serialization::serialization_result_ok(
      caors::serialization::class_<enemy>().commit()));
}

} // namespace caors::tests::serialization::polymorphic_object
