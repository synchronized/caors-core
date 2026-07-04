// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/object.h"

#include <catch2/catch_all.hpp>

#include <cstdint>
#include <limits>
#include <string_view>
#include <unordered_map>

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
  static constexpr caors::type_guid object_type_guid = guid_owner_actor::object_type_guid;
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

class managed_actor : public caors::object_base<managed_actor> {
public:
  static constexpr std::string_view object_type_name = "caors.test.object.managed_actor";

  int value = 0;
};

class managed_player : public caors::object_base<managed_player, managed_actor> {
public:
  static constexpr std::string_view object_type_name = "caors.test.object.managed_player";
};

class managed_other_actor : public caors::object_base<managed_other_actor> {
public:
  static constexpr std::string_view object_type_name = "caors.test.object.managed_other_actor";
};

class managed_non_default_object : public caors::object_base<managed_non_default_object> {
public:
  static constexpr std::string_view object_type_name = "caors.test.object.managed_non_default";

  explicit managed_non_default_object(int value) : value(value) {}

  int value = 0;
};

class unregistered_managed_actor : public caors::object_base<unregistered_managed_actor> {
public:
  static constexpr std::string_view object_type_name = "caors.test.object.unregistered_managed";
};

class destruction_observer : public caors::object_base<destruction_observer> {
public:
  static constexpr std::string_view object_type_name = "caors.test.object.destruction_observer";

  ~destruction_observer() override {
    invisible_during_destruction = manager != nullptr && manager->find(id()) == nullptr;
  }

  inline static caors::objects::manager* manager = nullptr;
  inline static bool invisible_during_destruction = false;
};

constexpr caors::object_guid test_object_guid_a{caors::guid::from_bytes(
    caors::guid::byte_array{0x18, 0x50, 0xf2, 0x4f, 0x4f, 0xf8, 0x47, 0x66, 0x9f, 0x38, 0x2b, 0x79,
                            0xc9, 0xd7, 0x3a, 0x01})};

constexpr caors::object_guid test_object_guid_b{caors::guid::from_bytes(
    caors::guid::byte_array{0x18, 0x50, 0xf2, 0x4f, 0x4f, 0xf8, 0x47, 0x66, 0x9f, 0x38, 0x2b, 0x79,
                            0xc9, 0xd7, 0x3a, 0x02})};

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
  const caors::object_id generated = caors::objects::generate_object_id();
  runtime_actor first;
  runtime_actor second;

  CHECK(static_cast<bool>(generated));
  CHECK(static_cast<bool>(first.id()));
  CHECK(static_cast<bool>(second.id()));
  CHECK(generated != first.id());
  CHECK(generated != second.id());
  CHECK(first.id() != second.id());
}

TEST_CASE("object - object_id supports standard unordered containers", "[object]") {
  const caors::object_id id = caors::objects::generate_object_id();
  REQUIRE(id);

  std::unordered_map<caors::object_id, int> values;
  values.emplace(id, 7);

  CHECK(values.at(id) == 7);
}

TEST_CASE("object - object index maps persistent guid to runtime id", "[object]") {
  caors::objects::object_index index;
  const caors::object_id first = caors::objects::generate_object_id();
  const caors::object_id second = caors::objects::generate_object_id();

  REQUIRE(first);
  REQUIRE(second);
  REQUIRE(first != second);

  CHECK(index.bind(test_object_guid_a, first));
  CHECK(index.bind(test_object_guid_a, first));
  CHECK(index.size() == 1);
  CHECK(index.contains(test_object_guid_a));
  CHECK(index.contains(first));
  CHECK(index.find(test_object_guid_a) == first);
  CHECK(index.guid_of(first) == test_object_guid_a);

  CHECK_FALSE(index.bind(test_object_guid_a, second));
  CHECK_FALSE(index.bind(test_object_guid_b, first));
  CHECK_FALSE(index.bind(caors::object_guid::nil(), second));
  CHECK_FALSE(index.bind(test_object_guid_b, caors::object_id::invalid()));

  CHECK(index.unbind(first));
  CHECK(index.empty());
  CHECK_FALSE(index.contains(test_object_guid_a));
  CHECK(index.find(test_object_guid_a) == caors::object_id::invalid());
  CHECK(index.guid_of(first) == caors::object_guid::nil());
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
  REQUIRE(type);
  CHECK(type.id() == registry_actor::static_type_id());
  CHECK(type.name() == registry_actor::object_type_name);
  CHECK_FALSE(type.guid());
  CHECK(type.size() == sizeof(registry_actor));
  CHECK(type.alignment() == alignof(registry_actor));
  CHECK(type.constructible());

  auto by_name = caors::objects::find(registry_actor::object_type_name);
  REQUIRE(by_name);
  CHECK(by_name.id() == registry_actor::static_type_id());
}

TEST_CASE("object - type guid can be provided by object subclass", "[object]") {
  CHECK(persistent_actor::static_type_guid() == persistent_actor::object_type_guid);

  REQUIRE(register_object_test_type<persistent_actor>(persistent_actor::object_type_name) ==
          caors::error{});

  auto stored = caors::objects::find<persistent_actor>();
  REQUIRE(stored);
  CHECK(stored.guid() == persistent_actor::object_type_guid);
}

TEST_CASE("object - registry reports registration errors", "[object]") {
  CHECK(caors::objects::class_<error_actor>("caors.test.object.missing_parent_child")
            .parent(caors::type_id{std::numeric_limits<std::uint64_t>::max()})
            .commit() == object_error(caors::errc::missing_parent));

  REQUIRE(register_object_test_type<error_actor>(error_actor::object_type_name) == caors::error{});

  CHECK(register_object_test_type<error_player>(error_actor::object_type_name) ==
        object_error(caors::errc::duplicate_name));
}

TEST_CASE("object - registry reports duplicate type guid", "[object]") {
  REQUIRE(register_object_test_type<guid_owner_actor>(guid_owner_actor::object_type_name) ==
          caors::error{});

  CHECK(register_object_test_type<duplicate_guid_player>(duplicate_guid_player::object_type_name) ==
        object_error(caors::errc::duplicate_guid));
}

TEST_CASE("object - registry checks parent relationship", "[object]") {
  REQUIRE(register_object_test_type<relation_actor>(relation_actor::object_type_name) ==
          caors::error{});
  const auto object_type = caors::objects::find<caors::object>();
  const auto actor_type = caors::objects::find<relation_actor>();
  REQUIRE(object_type);
  REQUIRE(actor_type);

  REQUIRE(register_object_test_type<relation_player>(relation_player::object_type_name,
                                                     actor_type.id()) == caors::error{});
  const auto player_type = caors::objects::find<relation_player>();
  REQUIRE(player_type);

  CHECK(caors::objects::is_a(player_type.id(), player_type.id()));
  CHECK(caors::objects::is_a(player_type.id(), actor_type.id()));
  CHECK(caors::objects::is_a(player_type.id(), object_type.id()));
  CHECK_FALSE(caors::objects::is_a(actor_type.id(), player_type.id()));
}

TEST_CASE("object - registry creates and destroys registered objects", "[object]") {
  caors::objects::registry& registry = caors::objects::get_registry();
  caors::tracking_resource tracking(&caors::get_system_memory_resource());

  REQUIRE(register_object_test_type<factory_actor>(factory_actor::object_type_name) ==
          caors::error{});

  const auto actor_type = caors::objects::find<factory_actor>();
  REQUIRE(actor_type);

  caors::object* instance = registry.create(actor_type.id(), &tracking);

  REQUIRE(instance != nullptr);
  CHECK(instance->type_id() == actor_type.id());
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
  REQUIRE(type);
  CHECK_FALSE(type.constructible());
  CHECK(registry.try_create<registered_non_default_object>() == nullptr);
  CHECK(registry.create<registered_non_default_object>() == nullptr);
}

TEST_CASE("object - direct creation supports constructor arguments", "[object]") {
  caors::tracking_resource tracking(&caors::get_system_memory_resource());

  auto* instance = caors::objects::create<direct_non_default_object>(&tracking, 42);

  REQUIRE(instance != nullptr);
  CHECK(instance->value == 42);
  CHECK(instance->type_id() == direct_non_default_object::static_type_id());

  caors::objects::destroy(instance, &tracking);
  CHECK(tracking.statistics().bytes_in_use == 0);
}

TEST_CASE("object - manager owns objects and resolves handles", "[object]") {
  caors::tracking_resource tracking(&caors::get_system_memory_resource());

  {
    caors::objects::manager manager(&tracking);

    const caors::error actor_status =
        register_object_test_type<managed_actor>(managed_actor::object_type_name);
    REQUIRE((actor_status == caors::error{} ||
             actor_status == object_error(caors::errc::duplicate_id)));

    managed_actor* instance = manager.create<managed_actor>();
    REQUIRE(instance != nullptr);
    instance->value = 13;

    const caors::object_id id = instance->id();
    const caors::objects::handle handle = manager.handle_of(id);

    CHECK(manager.size() == 1);
    CHECK(manager.contains(id));
    CHECK(handle);
    CHECK(handle.id() == id);
    CHECK(handle.type() == managed_actor::static_type_id());
    CHECK(manager.find(id) == instance);
    CHECK(manager.find(handle) == instance);
    CHECK(manager.find<managed_actor>(id) == instance);
    CHECK(manager.find<managed_actor>(handle) == instance);
    CHECK(manager.alive(handle));

    CHECK(manager.destroy(id));
    CHECK(manager.empty());
    CHECK_FALSE(manager.contains(id));
    CHECK_FALSE(manager.alive(handle));
    CHECK(manager.find(id) == nullptr);
    CHECK(manager.find(handle) == nullptr);
    CHECK_FALSE(manager.destroy(id));
  }

  CHECK(tracking.statistics().bytes_in_use == 0);
}

TEST_CASE("object - manager binds persistent object guids through index", "[object]") {
  caors::objects::object_index index;
  caors::objects::manager manager(index);

  const caors::error actor_status =
      register_object_test_type<managed_actor>(managed_actor::object_type_name);
  REQUIRE(
      (actor_status == caors::error{} || actor_status == object_error(caors::errc::duplicate_id)));

  managed_actor* instance = manager.create_with_guid<managed_actor>(test_object_guid_a);
  REQUIRE(instance != nullptr);

  const caors::object_id id = instance->id();
  CHECK(index.find(test_object_guid_a) == id);
  CHECK(index.guid_of(id) == test_object_guid_a);
  CHECK(manager.guid_of(id) == test_object_guid_a);
  CHECK(manager.find(test_object_guid_a) == instance);
  CHECK_FALSE(manager.bind_guid(id, test_object_guid_b));

  CHECK(manager.destroy(id));
  CHECK(index.empty());
  CHECK(manager.find(test_object_guid_a) == nullptr);
}

TEST_CASE("object - manager resolves typed weak handles and inheritance", "[object]") {
  caors::objects::manager manager;

  const caors::error actor_status =
      register_object_test_type<managed_actor>(managed_actor::object_type_name);
  REQUIRE(
      (actor_status == caors::error{} || actor_status == object_error(caors::errc::duplicate_id)));
  const caors::error player_status =
      register_object_test_type<managed_player>(managed_player::object_type_name);
  REQUIRE((player_status == caors::error{} ||
           player_status == object_error(caors::errc::duplicate_id)));

  managed_player* player = manager.create<managed_player>();
  REQUIRE(player != nullptr);

  const caors::object_id id = player->id();
  const caors::objects::weak_handle<managed_player> player_weak = manager.weak<managed_player>(id);
  const caors::objects::weak_handle<managed_actor> actor_weak = manager.weak<managed_actor>(id);

  REQUIRE(player_weak);
  REQUIRE(actor_weak);
  CHECK(manager.find(player_weak) == player);
  CHECK(manager.find(actor_weak) == static_cast<managed_actor*>(player));
  CHECK(manager.find(caors::objects::handle{id, managed_actor::static_type_id()}) == player);

  CHECK(manager.destroy(player));
  CHECK(manager.find(player_weak) == nullptr);
  CHECK(manager.find(actor_weak) == nullptr);
  CHECK_FALSE(manager.alive(player_weak.handle()));
}

TEST_CASE("object - manager supports registered non default constructible objects", "[object]") {
  caors::objects::manager manager;

  REQUIRE(register_object_test_type<managed_non_default_object>(
              managed_non_default_object::object_type_name) == caors::error{});

  CHECK(manager.create(managed_non_default_object::static_type_id()) == nullptr);

  managed_non_default_object* instance = manager.create<managed_non_default_object>(42);
  REQUIRE(instance != nullptr);
  CHECK(instance->value == 42);
  CHECK(manager.find<managed_non_default_object>(instance->id()) == instance);

  CHECK(manager.destroy(instance));
  CHECK(manager.empty());
}

TEST_CASE("object - manager rejects unregistered and foreign objects", "[object]") {
  caors::objects::manager manager;

  CHECK(manager.create<unregistered_managed_actor>() == nullptr);

  const caors::error actor_status =
      register_object_test_type<managed_actor>(managed_actor::object_type_name);
  REQUIRE(
      (actor_status == caors::error{} || actor_status == object_error(caors::errc::duplicate_id)));

  managed_actor external{};
  CHECK_FALSE(manager.destroy(&external));
  CHECK_FALSE(manager.contains(external.id()));
  CHECK(manager.find(external.id()) == nullptr);
}

TEST_CASE("object - manager rejects mismatched typed lookups", "[object]") {
  caors::objects::manager manager;

  const caors::error actor_status =
      register_object_test_type<managed_actor>(managed_actor::object_type_name);
  REQUIRE(
      (actor_status == caors::error{} || actor_status == object_error(caors::errc::duplicate_id)));
  REQUIRE(register_object_test_type<managed_other_actor>(managed_other_actor::object_type_name) ==
          caors::error{});

  managed_actor* instance = manager.create<managed_actor>();
  REQUIRE(instance != nullptr);

  CHECK(manager.find<managed_other_actor>(instance->id()) == nullptr);
  CHECK_FALSE(manager.weak<managed_other_actor>(instance->id()));
  CHECK(manager.find(caors::objects::handle{instance->id(),
                                            managed_other_actor::static_type_id()}) == nullptr);
}

TEST_CASE("object - manager removes lookup entry before destruction", "[object]") {
  caors::objects::manager manager;

  const caors::error register_status =
      register_object_test_type<destruction_observer>(destruction_observer::object_type_name);
  REQUIRE((register_status == caors::error{} ||
           register_status == object_error(caors::errc::duplicate_id)));

  destruction_observer::manager = &manager;
  destruction_observer::invisible_during_destruction = false;

  destruction_observer* instance = manager.create<destruction_observer>();
  REQUIRE(instance != nullptr);

  const caors::object_id id = instance->id();
  REQUIRE(manager.find(id) == instance);

  CHECK(manager.destroy(id));
  CHECK(destruction_observer::invisible_during_destruction);
  CHECK(manager.find(id) == nullptr);

  destruction_observer::manager = nullptr;
}

TEST_CASE("object - manager clear releases objects and invalidates handles", "[object]") {
  caors::tracking_resource tracking(&caors::get_system_memory_resource());

  {
    caors::objects::manager manager(&tracking);

    const caors::error actor_status =
        register_object_test_type<managed_actor>(managed_actor::object_type_name);
    REQUIRE((actor_status == caors::error{} ||
             actor_status == object_error(caors::errc::duplicate_id)));
    const caors::error player_status =
        register_object_test_type<managed_player>(managed_player::object_type_name);
    REQUIRE((player_status == caors::error{} ||
             player_status == object_error(caors::errc::duplicate_id)));

    managed_actor* actor = manager.create<managed_actor>();
    managed_player* player = manager.create<managed_player>();
    REQUIRE(actor != nullptr);
    REQUIRE(player != nullptr);

    const caors::objects::handle actor_handle = manager.handle_of(actor->id());
    const caors::objects::handle player_handle = manager.handle_of(player->id());

    REQUIRE(manager.size() == 2);
    REQUIRE(actor_handle);
    REQUIRE(player_handle);

    manager.clear();

    CHECK(manager.empty());
    CHECK_FALSE(manager.alive(actor_handle));
    CHECK_FALSE(manager.alive(player_handle));
    CHECK(manager.find(actor_handle) == nullptr);
    CHECK(manager.find(player_handle) == nullptr);
  }

  CHECK(tracking.statistics().bytes_in_use == 0);
}

TEST_CASE("object - manager clear unbinds persistent object guids", "[object]") {
  caors::objects::object_index index;
  caors::objects::manager manager(index);

  const caors::error actor_status =
      register_object_test_type<managed_actor>(managed_actor::object_type_name);
  REQUIRE(
      (actor_status == caors::error{} || actor_status == object_error(caors::errc::duplicate_id)));
  const caors::error player_status =
      register_object_test_type<managed_player>(managed_player::object_type_name);
  REQUIRE((player_status == caors::error{} ||
           player_status == object_error(caors::errc::duplicate_id)));

  managed_actor* actor = manager.create_with_guid<managed_actor>(test_object_guid_a);
  managed_player* player = manager.create_with_guid<managed_player>(test_object_guid_b);
  REQUIRE(actor != nullptr);
  REQUIRE(player != nullptr);
  REQUIRE(index.size() == 2);

  manager.clear();

  CHECK(manager.empty());
  CHECK(index.empty());
  CHECK(manager.find(test_object_guid_a) == nullptr);
  CHECK(manager.find(test_object_guid_b) == nullptr);
}
