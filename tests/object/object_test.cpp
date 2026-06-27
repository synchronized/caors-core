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

  CHECK(caors::register_object_type<actor>(registry, actor::object_type_name));
  CHECK(!caors::register_object_type<actor>(registry, actor::object_type_name));

  auto type = registry.find_type(actor::static_type_id());
  REQUIRE(type.has_value());
  CHECK(type->id == actor::static_type_id());
  CHECK(type->name == actor::object_type_name);
  CHECK(type->size == sizeof(actor));
  CHECK(type->alignment == alignof(actor));
  CHECK(type->constructible());

  auto by_name = registry.find_type(actor::object_type_name);
  REQUIRE(by_name.has_value());
  CHECK(by_name->id == actor::static_type_id());
}

TEST_CASE("object - registry checks parent relationship", "[object]") {
  caors::object_registry registry;

  REQUIRE(caors::register_object_type<actor>(registry, actor::object_type_name));
  REQUIRE(caors::register_object_type<player>(registry, player::object_type_name,
                                              actor::static_type_id()));

  CHECK(registry.is_a(player::static_type_id(), player::static_type_id()));
  CHECK(registry.is_a(player::static_type_id(), actor::static_type_id()));
  CHECK(registry.is_a(player::static_type_id(), caors::object::static_type_id()));
  CHECK_FALSE(registry.is_a(actor::static_type_id(), player::static_type_id()));
}

TEST_CASE("object - registry creates and destroys registered objects", "[object]") {
  caors::object_registry registry;
  caors::tracking_resource tracking(&caors::get_system_memory_resource());

  REQUIRE(caors::register_object_type<actor>(registry, actor::object_type_name));

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
                                                          non_default_object::object_type_name));

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
