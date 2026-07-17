// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/memory.h"
#include "caors/object.h"
#include "caors/runtime.h"

#include <catch2/catch_all.hpp>

#include <array>

namespace {

struct runtime_actor {
  int health = 7;

  int add_health(int delta) {
    health += delta;
    return health;
  }

  int read_health() const { return health; }
};

struct runtime_default_create_type {
  int value = 11;
};

struct runtime_factory_create_type {
  runtime_factory_create_type() = default;
  explicit runtime_factory_create_type(int input) : value(input) {}

  int value = 1;
};

struct runtime_resource_factory_create_type {
  explicit runtime_resource_factory_create_type(int input) : value(input) {}

  int value = 0;
};

struct runtime_constructor_create_type {
  explicit runtime_constructor_create_type(int input) : value(input) {}

  int value = 0;
};

struct runtime_factory_constructor_priority_type {
  runtime_factory_constructor_priority_type() = default;

  int value = 3;
};

struct runtime_missing_create_type {
  explicit runtime_missing_create_type(int input) : value(input) {}

  int value = 0;
};

class runtime_instance_owner : public caors::object_base<runtime_instance_owner> {
public:
  static constexpr std::string_view object_type_name = "caors.test.runtime.instance_owner";

  int health = 7;

  int add_health(int delta) {
    health += delta;
    return health;
  }

  int read_health() const { return health; }
};

struct runtime_const_owner {
  int health = 7;

  int add_health(int delta) {
    health += delta;
    return health;
  }

  int read_health() const { return health; }
};

enum class runtime_state {
  idle,
  running,
};

void* construct_runtime_factory_create_type(caors::memory_resource* resource) {
  if (resource != nullptr) {
    return caors::memory::make<runtime_factory_create_type>(resource, 42);
  }

  return new runtime_factory_create_type(42);
}

void destroy_runtime_factory_create_type(caors::memory_resource* resource, void* value) noexcept {
  auto* typed = static_cast<runtime_factory_create_type*>(value);
  if (resource != nullptr) {
    caors::memory::destroy(typed, resource);
    return;
  }

  delete typed;
}

void* construct_runtime_resource_factory_create_type(caors::memory_resource* resource) {
  if (resource != nullptr) {
    return caors::memory::make<runtime_resource_factory_create_type>(resource, 43);
  }

  return new runtime_resource_factory_create_type(43);
}

void destroy_runtime_resource_factory_create_type(caors::memory_resource* resource,
                                                  void* value) noexcept {
  auto* typed = static_cast<runtime_resource_factory_create_type*>(value);
  if (resource != nullptr) {
    caors::memory::destroy(typed, resource);
    return;
  }

  delete typed;
}

void* construct_runtime_factory_constructor_priority_type(caors::memory_resource* resource) {
  auto* value = caors::memory::make<runtime_factory_constructor_priority_type>(resource);
  if (value != nullptr) {
    value->value = 44;
  }
  return value;
}

void destroy_runtime_factory_constructor_priority_type(caors::memory_resource* resource,
                                                       void* value) noexcept {
  caors::memory::destroy(static_cast<runtime_factory_constructor_priority_type*>(value), resource);
}

} // namespace

TEST_CASE("runtime facade - class builder registers meta fields and methods", "[runtime]") {
  const caors::error status = caors::runtime::class_<runtime_actor>("caors.test.runtime.actor")
                                  .property("health", &runtime_actor::health)
                                  .method("add_health", &runtime_actor::add_health)
                                  .commit();
  CHECK_FALSE(status);

  caors::runtime::type type = caors::runtime::find<runtime_actor>();
  REQUIRE(type.valid());
  CHECK(type.name() == "caors.test.runtime.actor");
  CHECK(type.id() == caors::meta::id_of<runtime_actor>());
  CHECK(type.size() == sizeof(runtime_actor));
  CHECK(type.alignment() == alignof(runtime_actor));
  CHECK(type.value_constructible());
  CHECK(type.property_count() == 1);
  CHECK(type.method_count() == 1);

  caors::runtime::property field = type.find_property("health");
  REQUIRE(field.valid());

  runtime_actor actor{};
  CHECK(field.try_get<int>(&actor) == 7);
  CHECK(field.try_set(&actor, 11));
  CHECK(actor.health == 11);

  caors::runtime::method method = type.find_method("add_health").single();
  REQUIRE(method.valid());

  int delta = 5;
  std::array<caors::any_ref, 1> arguments{caors::any_ref::from(delta)};
  const std::optional<int> result = method.try_invoke<int>(&actor, arguments);
  REQUIRE(result.has_value());
  CHECK(*result == 16);
  CHECK(actor.health == 16);

  caors::any value = type.create_value();
  REQUIRE(value.valid());
  auto* constructed = value.try_get_mutable<runtime_actor>();
  REQUIRE(constructed != nullptr);
  CHECK(constructed->health == 7);
}

TEST_CASE("runtime facade - create uses registered factory before default construction",
          "[runtime]") {
  const caors::error status =
      caors::runtime::class_<runtime_factory_create_type>("caors.test.runtime.factory_create")
          .factory(&construct_runtime_factory_create_type, &destroy_runtime_factory_create_type)
          .commit();
  CHECK_FALSE(status);

  caors::runtime::type type = caors::runtime::find<runtime_factory_create_type>();
  REQUIRE(type.valid());
  CHECK(type.constructible());
  CHECK(type.value_constructible());

  caors::any value = type.create();
  REQUIRE(value.valid());
  auto* created = value.try_get_mutable<runtime_factory_create_type>();
  REQUIRE(created != nullptr);
  CHECK(created->value == 42);

  caors::any default_value = type.create_value();
  REQUIRE(default_value.valid());
  auto* default_created = default_value.try_get_mutable<runtime_factory_create_type>();
  REQUIRE(default_created != nullptr);
  CHECK(default_created->value == 1);
}

TEST_CASE("runtime facade - create falls back to descriptor default construction", "[runtime]") {
  const caors::error status =
      caors::runtime::class_<runtime_default_create_type>("caors.test.runtime.default_create")
          .commit();
  CHECK_FALSE(status);

  caors::runtime::type type = caors::runtime::find<runtime_default_create_type>();
  REQUIRE(type.valid());
  CHECK(type.constructible());
  CHECK(type.value_constructible());

  caors::any value = type.create();
  REQUIRE(value.valid());
  auto* created = value.try_get_mutable<runtime_default_create_type>();
  REQUIRE(created != nullptr);
  CHECK(created->value == 11);
}

TEST_CASE("runtime facade - create reports non constructible types", "[runtime]") {
  const caors::error status =
      caors::runtime::class_<runtime_missing_create_type>("caors.test.runtime.missing_create")
          .commit();
  CHECK_FALSE(status);

  caors::runtime::type type = caors::runtime::find<runtime_missing_create_type>();
  REQUIRE(type.valid());
  CHECK_FALSE(type.constructible());
  CHECK_FALSE(type.value_constructible());
  CHECK_FALSE(type.create());
  CHECK_FALSE(type.create_value());
}

TEST_CASE("runtime facade - create keeps factory resource ownership", "[runtime]") {
  const caors::error status = caors::runtime::class_<runtime_resource_factory_create_type>(
                                  "caors.test.runtime.resource_factory_create")
                                  .factory(&construct_runtime_resource_factory_create_type,
                                           &destroy_runtime_resource_factory_create_type)
                                  .commit();
  CHECK_FALSE(status);

  caors::runtime::type type = caors::runtime::find<runtime_resource_factory_create_type>();
  REQUIRE(type.valid());
  CHECK(type.constructible());
  CHECK_FALSE(type.value_constructible());

  caors::tracking_resource tracking{&caors::get_system_memory_resource()};

  caors::any value = type.create(&tracking);
  REQUIRE(value.valid());
  auto* created = value.try_get_mutable<runtime_resource_factory_create_type>();
  REQUIRE(created != nullptr);
  CHECK(created->value == 43);
  CHECK(tracking.statistics().allocation_count == 1);
  CHECK(tracking.statistics().bytes_in_use >= sizeof(runtime_resource_factory_create_type));

  value.reset();
  CHECK(tracking.statistics().deallocation_count == 1);
  CHECK(tracking.statistics().bytes_in_use == 0);
}

TEST_CASE("runtime facade - create uses registered constructor arguments", "[runtime]") {
  const caors::error status = caors::runtime::class_<runtime_constructor_create_type>(
                                  "caors.test.runtime.constructor_create")
                                  .constructor<int>()
                                  .commit();
  CHECK_FALSE(status);

  caors::runtime::type type = caors::runtime::find<runtime_constructor_create_type>();
  REQUIRE(type.valid());
  CHECK_FALSE(type.constructible());
  CHECK(type.constructible(1));
  CHECK_FALSE(type.value_constructible());
  CHECK_FALSE(type.create());

  int input = 37;
  std::array<caors::any_ref, 1> arguments{caors::any_ref::from(input)};
  caors::any value =
      type.create(std::span<const caors::any_ref>{arguments.data(), arguments.size()});
  REQUIRE(value.valid());
  auto* created = value.try_get_mutable<runtime_constructor_create_type>();
  REQUIRE(created != nullptr);
  CHECK(created->value == 37);

  float wrong = 3.0F;
  std::array<caors::any_ref, 1> wrong_arguments{caors::any_ref::from(wrong)};
  CHECK_FALSE(
      type.create(std::span<const caors::any_ref>{wrong_arguments.data(), wrong_arguments.size()}));
}

TEST_CASE("runtime facade - create keeps factory before zero argument constructor", "[runtime]") {
  const caors::error status = caors::runtime::class_<runtime_factory_constructor_priority_type>(
                                  "caors.test.runtime.factory_constructor_priority")
                                  .factory(&construct_runtime_factory_constructor_priority_type,
                                           &destroy_runtime_factory_constructor_priority_type)
                                  .constructor<>()
                                  .commit();
  CHECK_FALSE(status);

  caors::runtime::type type = caors::runtime::find<runtime_factory_constructor_priority_type>();
  REQUIRE(type.valid());
  CHECK(type.constructible());
  CHECK(type.constructible(0));

  caors::any value = type.create();
  REQUIRE(value.valid());
  auto* created = value.try_get_mutable<runtime_factory_constructor_priority_type>();
  REQUIRE(created != nullptr);
  CHECK(created->value == 44);
}

TEST_CASE("runtime facade - enum builder registers meta enum", "[runtime]") {
  const caors::error status =
      caors::runtime::enum_<runtime_state>("caors.test.runtime.state").commit();
  CHECK_FALSE(status);

  caors::runtime::type type = caors::runtime::find<runtime_state>();
  REQUIRE(type.valid());
  CHECK(type.name() == "caors.test.runtime.state");
  CHECK(type.kind() == caors::meta::type_kind::enumeration);
}

TEST_CASE("runtime facade - instance reads writes and calls reflected members", "[runtime]") {
  const caors::error status =
      caors::runtime::class_<runtime_instance_owner>(runtime_instance_owner::object_type_name)
          .property("health", &runtime_instance_owner::health)
          .method("add_health", &runtime_instance_owner::add_health)
          .method("read_health", &runtime_instance_owner::read_health)
          .commit();
  CHECK_FALSE(status);

  runtime_instance_owner owner{};
  caors::runtime::instance instance = caors::runtime::view(owner);
  REQUIRE(instance);
  CHECK(instance.type().name() == runtime_instance_owner::object_type_name);
  CHECK(instance.property_count() == 1);
  CHECK(instance.method_count() == 2);
  CHECK(instance.properties().size() == 1);
  CHECK(instance.methods().size() == 2);

  const caors::any health = instance.get("health");
  REQUIRE(health.valid());
  REQUIRE(health.try_get<int>() != nullptr);
  CHECK(*health.try_get<int>() == 7);

  CHECK(instance.set("health", 11));
  CHECK(owner.health == 11);

  caors::any result = instance.call("add_health", 5);
  REQUIRE(result.valid());
  REQUIRE(result.try_get<int>() != nullptr);
  CHECK(*result.try_get<int>() == 16);
  CHECK(owner.health == 16);
}

TEST_CASE("runtime facade - instance handles invalid and missing members", "[runtime]") {
  caors::runtime::instance invalid{};
  CHECK_FALSE(invalid);
  CHECK_FALSE(invalid.type());
  CHECK(invalid.property_count() == 0);
  CHECK(invalid.method_count() == 0);
  CHECK_FALSE(invalid.find_property("missing"));
  CHECK_FALSE(invalid.find_method("missing"));
  CHECK_FALSE(invalid.get("missing"));
  CHECK_FALSE(invalid.get_ref("missing"));
  CHECK_FALSE(invalid.set("missing", 1));
  CHECK_FALSE(invalid.call("missing"));

  runtime_instance_owner owner{};
  caors::runtime::instance instance = caors::runtime::view(owner);
  CHECK_FALSE(instance.find_property("missing"));
  CHECK_FALSE(instance.find_method("missing"));
  CHECK_FALSE(instance.get("missing"));
  CHECK_FALSE(instance.get_ref("missing"));
  CHECK_FALSE(instance.set("missing", 1));
  CHECK_FALSE(instance.call("missing"));
}

TEST_CASE("runtime facade - const instance is read only", "[runtime]") {
  const caors::error status =
      caors::runtime::class_<runtime_const_owner>("caors.test.runtime.const_owner")
          .property("health", &runtime_const_owner::health)
          .method("add_health", &runtime_const_owner::add_health)
          .method("read_health", &runtime_const_owner::read_health)
          .commit();
  CHECK_FALSE(status);

  runtime_const_owner owner{};
  const runtime_const_owner& readonly = owner;

  caors::runtime::instance instance = caors::runtime::view_const(readonly);
  REQUIRE(instance);
  CHECK_FALSE(instance.value().writable());

  const caors::any health = instance.get("health");
  REQUIRE(health.valid());
  REQUIRE(health.try_get<int>() != nullptr);
  CHECK(*health.try_get<int>() == 7);

  const caors::any_ref health_ref = instance.get_ref("health");
  REQUIRE(health_ref.valid());
  CHECK_FALSE(health_ref.writable());
  CHECK(health_ref.try_get<int>() != nullptr);
  CHECK(health_ref.try_get_mutable<int>() == nullptr);

  CHECK_FALSE(instance.set("health", 11));
  CHECK(owner.health == 7);

  const caors::any readonly_result = instance.call("read_health");
  REQUIRE(readonly_result.valid());
  REQUIRE(readonly_result.try_get<int>() != nullptr);
  CHECK(*readonly_result.try_get<int>() == 7);

  int delta = 5;
  std::array<caors::any_ref, 1> arguments{caors::any_ref::from(delta)};
  CHECK_FALSE(instance.call("add_health",
                            std::span<const caors::any_ref>{arguments.data(), arguments.size()}));
  CHECK(owner.health == 7);
}
