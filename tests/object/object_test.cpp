// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/object.h"

#include <catch2/catch_all.hpp>

#include <string_view>

namespace {

class actor : public caors::object_base<actor> {
public:
  static constexpr std::string_view object_type_name = "caors.test.actor";
};

class player : public caors::object_base<player, actor> {
public:
  static constexpr std::string_view object_type_name = "caors.test.player";
};

class persistent_actor : public caors::object_base<persistent_actor> {
public:
  static constexpr std::string_view object_type_name = "caors.test.persistent_actor";
  static constexpr caors::type_guid object_type_guid{caors::guid::from_bytes(
      caors::guid::byte_array{0x92, 0x8d, 0x35, 0x70, 0x54, 0x68, 0x46, 0x5c, 0x9d, 0xc4, 0x30,
                              0x7f, 0xb7, 0x45, 0xf2, 0x2c})};
};

class non_default_object : public caors::object_base<non_default_object> {
public:
  static constexpr std::string_view object_type_name = "caors.test.non_default";

  explicit non_default_object(int value) : value(value) {}

  int value = 0;
};

} // namespace

TEST_CASE("object - generated ids are valid and unique", "[object]") {
  actor first;
  actor second;

  CHECK(static_cast<bool>(first.id()));
  CHECK(static_cast<bool>(second.id()));
  CHECK(first.id() != second.id());
}

TEST_CASE("object - typed base exposes runtime type information", "[object]") {
  player instance;
  caors::object& base = instance;

  CHECK(player::static_type_id() == caors::make_object_type_id(player::object_type_name));
  CHECK(base.type_id() == player::static_type_id());
  CHECK(base.type_name() == player::object_type_name);
}

TEST_CASE("object - registry registers and finds types", "[object]") {
  caors::object_registry registry;

  CHECK(caors::register_object_type<actor>(registry, actor::object_type_name) ==
        caors::object_register_result::success);
  CHECK(caors::register_object_type<actor>(registry, actor::object_type_name) ==
        caors::object_register_result::duplicate_id);

  auto type = registry.find_type(actor::static_type_id());
  REQUIRE(type.has_value());
  CHECK(type->info.id == actor::static_type_id());
  CHECK(type->info.name == actor::object_type_name);
  CHECK_FALSE(type->info.guid);
  CHECK(type->info.size == sizeof(actor));
  CHECK(type->info.alignment == alignof(actor));
  CHECK(type->constructible());

  auto by_name = registry.find_type(actor::object_type_name);
  REQUIRE(by_name.has_value());
  CHECK(by_name->info.id == actor::static_type_id());
}

TEST_CASE("object - type guid can be provided by object subclass", "[object]") {
  caors::object_registry registry;

  CHECK(persistent_actor::static_type_guid() == persistent_actor::object_type_guid);

  caors::object_type type =
      caors::make_object_type<persistent_actor>(persistent_actor::object_type_name);
  CHECK(type.info.guid == persistent_actor::object_type_guid);

  REQUIRE(
      caors::register_object_type<persistent_actor>(registry, persistent_actor::object_type_name) ==
      caors::object_register_result::success);

  auto stored = registry.find_type(persistent_actor::static_type_id());
  REQUIRE(stored.has_value());
  CHECK(stored->info.guid == persistent_actor::object_type_guid);
}

TEST_CASE("object - registry reports registration errors", "[object]") {
  caors::object_registry registry;

  CHECK(registry.register_type(caors::object_type{}) ==
        caors::object_register_result::invalid_type);

  caors::object_type missing_parent = caors::make_object_type<actor>(
      "caors.test.missing_parent_child", caors::make_object_type_id("caors.test.missing_parent"));
  CHECK(registry.register_type(missing_parent) == caors::object_register_result::missing_parent);

  REQUIRE(caors::register_object_type<actor>(registry, actor::object_type_name) ==
          caors::object_register_result::success);

  caors::object_type colliding = caors::make_object_type<player>(player::object_type_name);
  colliding.info.id = actor::static_type_id();
  CHECK(registry.register_type(colliding) == caors::object_register_result::hash_collision);

  caors::object_type duplicate_name =
      caors::make_object_type<player>("caors.test.unique_duplicate_name");
  duplicate_name.info.name = caors::string_name{actor::object_type_name};
  CHECK(registry.register_type(duplicate_name) == caors::object_register_result::duplicate_name);
}

TEST_CASE("object - registry reports duplicate type guid", "[object]") {
  caors::object_registry registry;

  REQUIRE(
      caors::register_object_type<persistent_actor>(registry, persistent_actor::object_type_name) ==
      caors::object_register_result::success);

  caors::object_type duplicate_guid =
      caors::make_object_type<player>("caors.test.duplicate_guid_player");
  duplicate_guid.info.guid = persistent_actor::object_type_guid;
  CHECK(registry.register_type(duplicate_guid) == caors::object_register_result::duplicate_guid);
}

TEST_CASE("object - registry checks parent relationship", "[object]") {
  caors::object_registry registry;

  REQUIRE(caors::register_object_type<actor>(registry, actor::object_type_name) ==
          caors::object_register_result::success);
  REQUIRE(caors::register_object_type<player>(registry, player::object_type_name,
                                              actor::static_type_id()) ==
          caors::object_register_result::success);

  CHECK(registry.is_a(player::static_type_id(), player::static_type_id()));
  CHECK(registry.is_a(player::static_type_id(), actor::static_type_id()));
  CHECK(registry.is_a(player::static_type_id(), caors::object::static_type_id()));
  CHECK_FALSE(registry.is_a(actor::static_type_id(), player::static_type_id()));
}

TEST_CASE("object - registry creates and destroys registered objects", "[object]") {
  caors::object_registry registry;
  caors::tracking_resource tracking(&caors::get_system_memory_resource());

  REQUIRE(caors::register_object_type<actor>(registry, actor::object_type_name) ==
          caors::object_register_result::success);

  caors::object* instance = registry.create(actor::static_type_id(), &tracking);

  REQUIRE(instance != nullptr);
  CHECK(instance->type_id() == actor::static_type_id());
  CHECK(tracking.statistics().allocation_count == 1);

  CHECK(registry.destroy(instance, &tracking));
  CHECK(tracking.statistics().deallocation_count == 1);
  CHECK(tracking.statistics().bytes_in_use == 0);
}

TEST_CASE("object - non default constructible types can be described but not factory-created",
          "[object]") {
  caors::object_registry registry;

  REQUIRE(caors::register_object_type<non_default_object>(registry,
                                                          non_default_object::object_type_name) ==
          caors::object_register_result::success);

  auto type = registry.find_type(non_default_object::static_type_id());
  REQUIRE(type.has_value());
  CHECK_FALSE(type->constructible());
  CHECK(registry.try_create(non_default_object::static_type_id()) == nullptr);
  CHECK(registry.create(non_default_object::static_type_id()) == nullptr);
}

TEST_CASE("object - direct creation supports constructor arguments", "[object]") {
  caors::tracking_resource tracking(&caors::get_system_memory_resource());

  auto* instance = caors::create_object<non_default_object>(&tracking, 42);

  REQUIRE(instance != nullptr);
  CHECK(instance->value == 42);
  CHECK(instance->type_id() == non_default_object::static_type_id());

  caors::destroy_object(instance, &tracking);
  CHECK(tracking.statistics().bytes_in_use == 0);
}
