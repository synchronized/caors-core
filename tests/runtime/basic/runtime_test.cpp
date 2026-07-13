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
};

struct runtime_target : caors::object {};

class runtime_instance_owner : public caors::object_base<runtime_instance_owner> {
public:
  static constexpr std::string_view object_type_name = "caors.test.runtime.instance_owner";

  int health = 7;

  int add_health(int delta) {
    health += delta;
    return health;
  }
};

class runtime_object_owner : public caors::object_base<runtime_object_owner> {
public:
  static constexpr std::string_view object_type_name = "caors.test.runtime.object_owner";

  caors::object_ptr<runtime_target> target{};
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
          .commit();
  CHECK_FALSE(status);

  runtime_instance_owner owner{};
  caors::runtime::instance instance = caors::runtime::view(owner);
  REQUIRE(instance);
  CHECK(instance.type().name() == runtime_instance_owner::object_type_name);
  CHECK(instance.field_count() == 1);
  CHECK(instance.method_count() == 1);
  CHECK(instance.fields().size() == 1);
  CHECK(instance.methods().size() == 1);

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
