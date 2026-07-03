// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/object.h"

#include <catch2/catch_all.hpp>

#include <cstdint>
#include <limits>
#include <string_view>

namespace {

class runtime_actor : public caors::object_base<runtime_actor> {
public:
  static constexpr std::string_view object_type_name = "caors.test.object.runtime_actor";
};

class runtime_player : public caors::object_base<runtime_player, runtime_actor> {
public:
  static constexpr std::string_view object_type_name = "caors.test.object.runtime_player";
};

class registry_actor : public caors::object_base<registry_actor> {
public:
  static constexpr std::string_view object_type_name = "caors.test.object.registry_actor";
};

class persistent_actor : public caors::object_base<persistent_actor> {
public:
  static constexpr std::string_view object_type_name = "caors.test.object.persistent_actor";
  static constexpr caors::type_guid object_type_guid{caors::guid::from_bytes(
      caors::guid::byte_array{0x92, 0x8d, 0x35, 0x70, 0x54, 0x68, 0x46, 0x5c, 0x9d, 0xc4, 0x30,
                              0x7f, 0xb7, 0x45, 0xf2, 0x2c})};
};

class error_actor : public caors::object_base<error_actor> {
public:
  static constexpr std::string_view object_type_name = "caors.test.object.error_actor";
};

class error_player : public caors::object_base<error_player, error_actor> {
public:
  static constexpr std::string_view object_type_name = "caors.test.object.error_player";
};

class guid_owner_actor : public caors::object_base<guid_owner_actor> {
public:
  static constexpr std::string_view object_type_name = "caors.test.object.guid_owner_actor";
  static constexpr caors::type_guid object_type_guid{caors::guid::from_bytes(
      caors::guid::byte_array{0xf8, 0x23, 0x42, 0x4e, 0xc4, 0x3b, 0x4a, 0xe2, 0x9a, 0x8e, 0x55,
                              0xc7, 0xe4, 0xf8, 0x3a, 0x11})};
};

class duplicate_guid_player : public caors::object_base<duplicate_guid_player> {
public:
  static constexpr std::string_view object_type_name = "caors.test.object.duplicate_guid_player";
};

class relation_actor : public caors::object_base<relation_actor> {
public:
  static constexpr std::string_view object_type_name = "caors.test.object.relation_actor";
};

class relation_player : public caors::object_base<relation_player, relation_actor> {
public:
  static constexpr std::string_view object_type_name = "caors.test.object.relation_player";
};

class factory_actor : public caors::object_base<factory_actor> {
public:
  static constexpr std::string_view object_type_name = "caors.test.object.factory_actor";
};

class registered_non_default_object : public caors::object_base<registered_non_default_object> {
public:
  static constexpr std::string_view object_type_name = "caors.test.object.registered_non_default";

  explicit registered_non_default_object(int value) : value(value) {}

  int value = 0;
};

class direct_non_default_object : public caors::object_base<direct_non_default_object> {
public:
  static constexpr std::string_view object_type_name = "caors.test.object.direct_non_default";

  explicit direct_non_default_object(int value) : value(value) {}

  int value = 0;
};

[[nodiscard]] constexpr caors::error object_error(caors::errc code) noexcept {
  return {caors::error_domain::object, code};
}

template <typename T>
[[nodiscard]] caors::error
register_object_test_type(std::string_view name,
                          caors::object_type_id parent_id = caors::object_type_id::invalid()) {
  auto builder = caors::objects::class_<T>(name);
  if (parent_id) {
    builder.parent(parent_id);
  }

  return builder.commit();
}

} // namespace

TEST_CASE("object - generated ids are valid and unique", "[object]") {
  runtime_actor first;
  runtime_actor second;

  CHECK(static_cast<bool>(first.id()));
  CHECK(static_cast<bool>(second.id()));
  CHECK(first.id() != second.id());
}

TEST_CASE("object - typed base exposes runtime type information", "[object]") {
  const caors::error actor_status =
      register_object_test_type<runtime_actor>(runtime_actor::object_type_name);
  REQUIRE(
      (actor_status == caors::error{} || actor_status == object_error(caors::errc::duplicate_id)));
  REQUIRE(register_object_test_type<runtime_player>(runtime_player::object_type_name) ==
          caors::error{});

  runtime_player instance;
  caors::object& base = instance;

  CHECK(runtime_player::static_type_id());
  CHECK(runtime_player::static_type_id() == caors::meta::id_of<runtime_player>());
  CHECK(base.type_id() == runtime_player::static_type_id());
  CHECK(base.type_name() == runtime_player::object_type_name);
}

TEST_CASE("object - handles carry identity without owning instances", "[object]") {
  const caors::error register_status =
      register_object_test_type<runtime_actor>(runtime_actor::object_type_name);
  REQUIRE((register_status == caors::error{} ||
           register_status == object_error(caors::errc::duplicate_id)));

  runtime_actor instance;

  const caors::objects::handle handle{instance.id(), instance.type_id()};
  CHECK(handle);
  CHECK(handle.id() == instance.id());
  CHECK(handle.type() == instance.type_id());
  CHECK(handle == caors::objects::handle{instance.id(), instance.type_id()});
  CHECK(handle != caors::objects::handle{caors::object_id{999}, instance.type_id()});

  caors::objects::weak_handle<runtime_actor> weak{instance};
  CHECK(weak);
  CHECK(weak.handle() == handle);

  weak.reset();
  CHECK_FALSE(weak);
}

TEST_CASE("object - registry registers and finds types", "[object]") {
  CHECK(register_object_test_type<registry_actor>(registry_actor::object_type_name) ==
        caors::error{});
  CHECK(register_object_test_type<registry_actor>(registry_actor::object_type_name) ==
        object_error(caors::errc::duplicate_id));

  auto type = caors::objects::find<registry_actor>();
  REQUIRE(type.has_value());
  CHECK(type->id() == registry_actor::static_type_id());
  CHECK(type->name() == registry_actor::object_type_name);
  CHECK_FALSE(type->guid());
  CHECK(type->size() == sizeof(registry_actor));
  CHECK(type->alignment() == alignof(registry_actor));
  CHECK(type->constructible());

  auto by_name = caors::objects::find(registry_actor::object_type_name);
  REQUIRE(by_name.has_value());
  CHECK(by_name->id() == registry_actor::static_type_id());
}

TEST_CASE("object - type guid can be provided by object subclass", "[object]") {
  caors::objects::registry& registry = caors::objects::get_registry();

  CHECK(persistent_actor::static_type_guid() == persistent_actor::object_type_guid);

  const caors::meta_type_data type =
      caors::make_object_meta_type_data<persistent_actor>(persistent_actor::object_type_name);
  CHECK(type.guid() == persistent_actor::object_type_guid);

  REQUIRE(register_object_test_type<persistent_actor>(persistent_actor::object_type_name) ==
          caors::error{});

  auto stored = caors::objects::find<persistent_actor>();
  REQUIRE(stored.has_value());
  CHECK(stored->guid() == persistent_actor::object_type_guid);
}

TEST_CASE("object - registry reports registration errors", "[object]") {
  caors::objects::registry& registry = caors::objects::get_registry();

  CHECK(registry.register_type(caors::meta_type_data{}, caors::objects::type_data{}) ==
        object_error(caors::errc::invalid_type));

  caors::meta_type_data missing_parent = caors::make_object_meta_type_data<error_actor>(
      "caors.test.object.missing_parent_child",
      caors::type_id{std::numeric_limits<std::uint64_t>::max()});
  CHECK(registry.register_type(missing_parent, caors::make_object_type_data<error_actor>()) ==
        object_error(caors::errc::missing_parent));

  REQUIRE(register_object_test_type<error_actor>(error_actor::object_type_name) == caors::error{});
  const auto actor_type = caors::objects::find<error_actor>();
  REQUIRE(actor_type.has_value());

  caors::meta_type_data colliding = caors::make_object_meta_type_data<error_player>(
      error_player::object_type_name, actor_type->id());
  colliding.set_id(actor_type->id());
  CHECK(registry.register_type(colliding, caors::make_object_type_data<error_player>()) ==
        object_error(caors::errc::invalid_type));

  caors::meta_type_data duplicate_name =
      caors::make_object_meta_type_data<error_player>("caors.test.object.unique_duplicate_name");
  duplicate_name.set_name(caors::string_name{error_actor::object_type_name});
  CHECK(registry.register_type(duplicate_name, caors::make_object_type_data<error_player>()) ==
        object_error(caors::errc::duplicate_name));
}

TEST_CASE("object - registry reports duplicate type guid", "[object]") {
  caors::objects::registry& registry = caors::objects::get_registry();

  REQUIRE(register_object_test_type<guid_owner_actor>(guid_owner_actor::object_type_name) ==
          caors::error{});

  caors::meta_type_data duplicate_guid = caors::make_object_meta_type_data<duplicate_guid_player>(
      duplicate_guid_player::object_type_name);
  duplicate_guid.set_guid(guid_owner_actor::object_type_guid);
  CHECK(registry.register_type(duplicate_guid,
                               caors::make_object_type_data<duplicate_guid_player>()) ==
        object_error(caors::errc::duplicate_guid));
}

TEST_CASE("object - registry checks parent relationship", "[object]") {
  caors::objects::registry& registry = caors::objects::get_registry();

  REQUIRE(register_object_test_type<relation_actor>(relation_actor::object_type_name) ==
          caors::error{});
  const auto object_type = caors::objects::find<caors::object>();
  const auto actor_type = caors::objects::find<relation_actor>();
  REQUIRE(object_type.has_value());
  REQUIRE(actor_type.has_value());

  REQUIRE(register_object_test_type<relation_player>(relation_player::object_type_name,
                                                     actor_type->id()) == caors::error{});
  const auto player_type = caors::objects::find<relation_player>();
  REQUIRE(player_type.has_value());

  CHECK(caors::objects::is_a(player_type->id(), player_type->id()));
  CHECK(caors::objects::is_a(player_type->id(), actor_type->id()));
  CHECK(caors::objects::is_a(player_type->id(), object_type->id()));
  CHECK_FALSE(caors::objects::is_a(actor_type->id(), player_type->id()));
}

TEST_CASE("object - registry creates and destroys registered objects", "[object]") {
  caors::objects::registry& registry = caors::objects::get_registry();
  caors::tracking_resource tracking(&caors::get_system_memory_resource());

  REQUIRE(register_object_test_type<factory_actor>(factory_actor::object_type_name) ==
          caors::error{});

  const auto actor_type = caors::objects::find<factory_actor>();
  REQUIRE(actor_type.has_value());

  caors::object* instance = registry.create(actor_type->id(), &tracking);

  REQUIRE(instance != nullptr);
  CHECK(instance->type_id() == actor_type->id());
  CHECK(tracking.statistics().allocation_count == 1);

  CHECK(registry.destroy(instance, &tracking));
  CHECK(tracking.statistics().deallocation_count == 1);
  CHECK(tracking.statistics().bytes_in_use == 0);
}

TEST_CASE("object - non default constructible types can be described but not factory-created",
          "[object]") {
  caors::objects::registry& registry = caors::objects::get_registry();

  REQUIRE(register_object_test_type<registered_non_default_object>(
              registered_non_default_object::object_type_name) == caors::error{});

  auto type = caors::objects::find<registered_non_default_object>();
  REQUIRE(type.has_value());
  CHECK_FALSE(type->constructible());
  CHECK(registry.try_create<registered_non_default_object>() == nullptr);
  CHECK(registry.create<registered_non_default_object>() == nullptr);
}

TEST_CASE("object - direct creation supports constructor arguments", "[object]") {
  caors::tracking_resource tracking(&caors::get_system_memory_resource());

  auto* instance = caors::create_object<direct_non_default_object>(&tracking, 42);

  REQUIRE(instance != nullptr);
  CHECK(instance->value == 42);
  CHECK(instance->type_id() == direct_non_default_object::static_type_id());

  caors::destroy_object(instance, &tracking);
  CHECK(tracking.statistics().bytes_in_use == 0);
}
