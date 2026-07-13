// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

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
