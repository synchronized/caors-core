// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

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

struct runtime_target : caors::object {};

struct runtime_plain_owner {
  int health = 7;
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

class runtime_object_owner : public caors::object_base<runtime_object_owner> {
public:
  static constexpr std::string_view object_type_name = "caors.test.runtime.object_owner";

  caors::object_ptr<runtime_target> target{};
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

} // namespace

TEST_CASE("runtime facade - class builder registers meta and reflection", "[runtime]") {
  const caors::error status = caors::runtime::class_<runtime_actor>("caors.test.runtime.actor")
                                  .field("health", &runtime_actor::health)
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
  CHECK(type.reflectable());
  CHECK(type.field_count() == 1);
  CHECK(type.method_count() == 1);

  caors::runtime::field field = type.find_field("health");
  REQUIRE(field.valid());

  runtime_actor actor{};
  CHECK(field.try_get<int>(&actor) == 7);
  CHECK(field.try_set(&actor, 11));
  CHECK(actor.health == 11);

  caors::runtime::method method = type.find_method("add_health");
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
          .field("health", &runtime_instance_owner::health)
          .method("add_health", &runtime_instance_owner::add_health)
          .method("read_health", &runtime_instance_owner::read_health)
          .commit();
  CHECK_FALSE(status);

  runtime_instance_owner owner{};
  caors::runtime::instance instance = caors::runtime::view(owner);
  REQUIRE(instance);
  CHECK(instance.type().name() == runtime_instance_owner::object_type_name);
  CHECK(instance.field_count() == 1);
  CHECK(instance.method_count() == 2);
  CHECK(instance.fields().size() == 1);
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
  CHECK(invalid.field_count() == 0);
  CHECK(invalid.method_count() == 0);
  CHECK_FALSE(invalid.find_field("missing"));
  CHECK_FALSE(invalid.find_method("missing"));
  CHECK_FALSE(invalid.get("missing"));
  CHECK_FALSE(invalid.get_ref("missing"));
  CHECK_FALSE(invalid.set("missing", 1));
  CHECK_FALSE(invalid.call("missing"));

  runtime_instance_owner owner{};
  caors::runtime::instance instance = caors::runtime::view(owner);
  CHECK_FALSE(instance.find_field("missing"));
  CHECK_FALSE(instance.find_method("missing"));
  CHECK_FALSE(instance.get("missing"));
  CHECK_FALSE(instance.get_ref("missing"));
  CHECK_FALSE(instance.set("missing", 1));
  CHECK_FALSE(instance.call("missing"));
}

TEST_CASE("runtime facade - const instance is read only", "[runtime]") {
  const caors::error status =
      caors::runtime::class_<runtime_const_owner>("caors.test.runtime.const_owner")
          .field("health", &runtime_const_owner::health)
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

TEST_CASE("runtime facade - object view reads and writes object pointer fields", "[runtime]") {
  const caors::error status =
      caors::runtime::class_<runtime_object_owner>(runtime_object_owner::object_type_name)
          .field("target", &runtime_object_owner::target)
          .commit();
  CHECK_FALSE(status);

  runtime_object_owner owner{};
  runtime_target target{};

  caors::runtime::object object = caors::runtime::object_view(owner);
  REQUIRE(object);
  CHECK(object.ptr() == &owner);
  CHECK(object.get_ptr("target") == nullptr);
  CHECK(object.set_ptr("target", &target));
  CHECK(owner.target.get() == &target);
  CHECK(object.get_ptr("target") == &target);
}

TEST_CASE("runtime facade - object view reports non object pointer fields as empty", "[runtime]") {
  const caors::error status =
      caors::runtime::class_<runtime_plain_owner>("caors.test.runtime.plain_owner")
          .field("health", &runtime_plain_owner::health)
          .commit();
  CHECK_FALSE(status);

  runtime_plain_owner owner{};

  caors::runtime::object object = caors::runtime::object_view(caors::runtime::view(owner));
  REQUIRE(object);
  CHECK(object.ptr() == nullptr);
  CHECK(object.get_ptr("health") == nullptr);
  CHECK_FALSE(object.set_ptr("health", nullptr));
  CHECK(owner.health == 7);
}
