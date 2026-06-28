// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/reflection.h"

#include <catch2/catch_all.hpp>

#include <cstddef>
#include <string_view>

namespace {

struct actor {
  int id = 0;
  float health = 0.0F;
};

struct player {
  int level = 0;
};

constexpr caors::type_guid actor_guid{caors::guid::from_bytes(
    caors::guid::byte_array{0x54, 0x20, 0x08, 0xa6, 0x08, 0x5e, 0x48, 0xb7, 0x9c, 0x2f, 0x88, 0x68,
                            0xb2, 0x42, 0x18, 0xf3})};

[[nodiscard]] caors::reflection_type make_actor_reflection() {
  return caors::make_reflection_type<actor>(
      "caors.test.reflection.actor", actor_guid,
      {
          caors::make_field_info<int>("id", "int", offsetof(actor, id)),
          caors::make_field_info<float>("health", "float", offsetof(actor, health)),
      });
}

} // namespace

TEST_CASE("reflection - field info describes field layout", "[reflection]") {
  const caors::field_info field =
      caors::make_field_info<float>("health", "float", offsetof(actor, health));

  CHECK(field.valid());
  CHECK(field.name == "health");
  CHECK(field.type == caors::make_type_id("float"));
  CHECK(field.type_name == "float");
  CHECK(field.offset == offsetof(actor, health));
  CHECK(field.size == sizeof(float));
  CHECK(field.alignment == alignof(float));
}

TEST_CASE("reflection - type stores and finds fields", "[reflection]") {
  const caors::reflection_type type = make_actor_reflection();

  CHECK(type.valid());
  CHECK(type.info.name == "caors.test.reflection.actor");
  CHECK(type.info.guid == actor_guid);
  CHECK(type.fields.size() == 2);

  auto health = type.find_field("health");
  REQUIRE(health.has_value());
  CHECK(health->type_name == "float");
  CHECK_FALSE(type.find_field("missing").has_value());
}

TEST_CASE("reflection - registry registers and finds types", "[reflection]") {
  caors::reflection_registry registry;

  REQUIRE(registry.register_type(make_actor_reflection()) ==
          caors::reflection_register_result::success);

  CHECK(registry.size() == 1);
  CHECK(registry.contains(caors::make_type_id("caors.test.reflection.actor")));

  auto by_name = registry.find_type("caors.test.reflection.actor");
  REQUIRE(by_name.has_value());
  CHECK(by_name->fields.size() == 2);

  auto by_guid = registry.find_type(actor_guid);
  REQUIRE(by_guid.has_value());
  CHECK(by_guid->info.id == by_name->info.id);
}

TEST_CASE("reflection - registry registers child type with parent", "[reflection]") {
  caors::reflection_registry registry;
  const caors::type_id actor_id = caors::make_type_id("caors.test.reflection.actor");

  REQUIRE(registry.register_type(make_actor_reflection()) ==
          caors::reflection_register_result::success);
  REQUIRE(caors::register_reflection<player>(
              registry, "caors.test.reflection.player",
              {
                  caors::make_field_info<int>("level", "int", offsetof(player, level)),
              },
              actor_id) == caors::reflection_register_result::success);

  auto reflected_player = registry.find_type("caors.test.reflection.player");
  REQUIRE(reflected_player.has_value());
  CHECK(reflected_player->info.parent_id == actor_id);
}

TEST_CASE("reflection - registry reports duplicate reflection", "[reflection]") {
  caors::reflection_registry registry;

  REQUIRE(registry.register_type(make_actor_reflection()) ==
          caors::reflection_register_result::success);
  CHECK(registry.register_type(make_actor_reflection()) ==
        caors::reflection_register_result::duplicate_reflection);
}

TEST_CASE("reflection - registry validates fields", "[reflection]") {
  caors::reflection_registry registry;

  caors::reflection_type invalid_field = caors::make_reflection_type<actor>(
      "caors.test.reflection.invalid_field", {
                                                 caors::field_info{},
                                             });
  CHECK(registry.register_type(invalid_field) == caors::reflection_register_result::invalid_field);

  caors::reflection_type duplicate_field = caors::make_reflection_type<actor>(
      "caors.test.reflection.duplicate_field",
      {
          caors::make_field_info<int>("id", "int", offsetof(actor, id)),
          caors::make_field_info<float>("id", "float", offsetof(actor, health)),
      });
  CHECK(registry.register_type(duplicate_field) ==
        caors::reflection_register_result::duplicate_field);
}

TEST_CASE("reflection - registry reports inconsistent existing type", "[reflection]") {
  caors::reflection_registry registry;

  REQUIRE(registry.register_type(make_actor_reflection()) ==
          caors::reflection_register_result::success);

  caors::reflection_type inconsistent = make_actor_reflection();
  inconsistent.info.size += 1;
  CHECK(registry.register_type(inconsistent) ==
        caors::reflection_register_result::inconsistent_type);
}
