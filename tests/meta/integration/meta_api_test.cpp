// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta.h"
#include "caors/object.h"
#include "caors/registration.h"

#include <catch2/catch_all.hpp>

#include <array>

namespace {

struct meta_api_actor {
  int health = 7;

  int add_health(int delta) {
    health += delta;
    return health;
  }
};

class meta_api_instance_owner : public caors::object_base<meta_api_instance_owner> {
public:
  static constexpr std::string_view object_type_name = "caors.test.meta.api.instance_owner";

  int health = 7;

  int add_health(int delta) {
    health += delta;
    return health;
  }

  int read_health() const { return health; }
};

struct meta_api_const_owner {
  int health = 7;

  int add_health(int delta) {
    health += delta;
    return health;
  }

  int read_health() const { return health; }
};

struct meta_api_cached_owner {
  int health = 7;

  int add_health(int delta) {
    health += delta;
    return health;
  }
};

enum class meta_api_state {
  idle,
  running,
};

} // namespace

TEST_CASE("meta api - class builder registers properties and methods", "[meta][api]") {
  const caors::error status =
      caors::registration::type_<meta_api_actor>("caors.test.meta.api.actor")
          .meta()
          .property("health", &meta_api_actor::health)
          .method("add_health", &meta_api_actor::add_health)
          .commit();
  CHECK_FALSE(status);

  caors::meta::type type = caors::meta::find<meta_api_actor>();
  REQUIRE(type.valid());
  CHECK(type.name() == "caors.test.meta.api.actor");
  CHECK(type.id() == caors::meta::id_of<meta_api_actor>());
  CHECK(type.size() == sizeof(meta_api_actor));
  CHECK(type.alignment() == alignof(meta_api_actor));
  CHECK(type.value_constructible());
  CHECK(type.property_count() == 1);
  CHECK(type.method_count() == 1);

  caors::meta::property property = type.find_property("health");
  REQUIRE(property.valid());

  meta_api_actor actor{};
  const caors::meta::value health = property.get_value(caors::meta::view(actor));
  REQUIRE(health.is_i64());
  CHECK(health.as_i64() == 7);
  CHECK(property.try_set_value(caors::meta::view(actor), caors::meta::value{11}));
  CHECK(actor.health == 11);

  caors::meta::method method = type.find_method("add_health").single();
  REQUIRE(method.valid());

  std::array<caors::meta::value, 1> arguments{caors::meta::value{5}};
  const caors::meta::invoke_value_result result =
      method.invoke(caors::meta::view(actor), arguments);
  REQUIRE(result.has_value());
  REQUIRE(result.value().is_i64());
  CHECK(result.value().as_i64() == 16);
  CHECK(actor.health == 16);
}

TEST_CASE("meta api - cached property and method handles support hot loops", "[meta][api]") {
  const caors::error status =
      caors::registration::type_<meta_api_cached_owner>("caors.test.meta.api.cached_owner")
          .meta()
          .property("health", &meta_api_cached_owner::health)
          .method("add_health", &meta_api_cached_owner::add_health)
          .commit();
  CHECK_FALSE(status);

  const caors::meta::type type = caors::meta::find<meta_api_cached_owner>();
  REQUIRE(type);

  const caors::meta::property health = type.find_property("health");
  const caors::meta::method_set add_health = type.find_method("add_health");
  REQUIRE(health);
  REQUIRE(add_health);

  meta_api_cached_owner owner{};
  const caors::meta::instance instance = caors::meta::view(owner);

  for (int index = 0; index < 3; ++index) {
    REQUIRE(health.try_set_value(instance, caors::meta::value{index}));
    const caors::meta::value current = health.get_value(instance);
    REQUIRE(current.is_i64());
    CHECK(current.as_i64() == index);

    std::array<caors::meta::value, 1> arguments{caors::meta::value{1}};
    const caors::meta::invoke_value_result result = add_health.invoke(instance, arguments);
    REQUIRE(result.has_value());
    REQUIRE(result.value().is_i64());
    CHECK(result.value().as_i64() == index + 1);
  }
}

TEST_CASE("meta api - enum builder registers enum type", "[meta][api]") {
  const caors::error status =
      caors::registration::enum_<meta_api_state>("caors.test.meta.api.state").commit();
  CHECK_FALSE(status);

  caors::meta::type type = caors::meta::find<meta_api_state>();
  REQUIRE(type.valid());
  CHECK(type.name() == "caors.test.meta.api.state");
  CHECK(type.kind() == caors::meta::type_kind::enumeration);
}

TEST_CASE("meta api - instance reads writes and calls reflected members", "[meta][api]") {
  const caors::error status =
      caors::registration::type_<meta_api_instance_owner>(meta_api_instance_owner::object_type_name)
          .meta()
          .property("health", &meta_api_instance_owner::health)
          .method("add_health", &meta_api_instance_owner::add_health)
          .method("read_health", &meta_api_instance_owner::read_health)
          .commit();
  CHECK_FALSE(status);

  meta_api_instance_owner owner{};
  caors::meta::instance instance = caors::meta::view(owner);
  REQUIRE(instance);
  CHECK(instance.type().name() == meta_api_instance_owner::object_type_name);
  CHECK(instance.property_count() == 1);
  CHECK(instance.method_count() == 2);
  CHECK(instance.properties().size() == 1);
  CHECK(instance.methods().size() == 2);

  const caors::meta::value health = instance.get("health");
  REQUIRE(health.is_i64());
  CHECK(health.as_i64() == 7);

  CHECK(instance.set("health", 11));
  CHECK(owner.health == 11);

  caors::meta::invoke_value_result result = instance.call("add_health", 5);
  REQUIRE(result.has_value());
  REQUIRE(result.value().is_i64());
  CHECK(result.value().as_i64() == 16);
  CHECK(owner.health == 16);
}

TEST_CASE("meta api - instance handles invalid and missing members", "[meta][api]") {
  caors::meta::instance invalid{};
  CHECK_FALSE(invalid);
  CHECK_FALSE(invalid.type());
  CHECK(invalid.property_count() == 0);
  CHECK(invalid.method_count() == 0);
  CHECK_FALSE(invalid.find_property("missing"));
  CHECK_FALSE(invalid.find_method("missing"));
  CHECK(invalid.get("missing").is_null());
  CHECK_FALSE(invalid.get_ref("missing"));
  CHECK_FALSE(invalid.set("missing", 1));
  CHECK_FALSE(invalid.call("missing"));

  meta_api_instance_owner owner{};
  caors::meta::instance instance = caors::meta::view(owner);
  CHECK_FALSE(instance.find_property("missing"));
  CHECK_FALSE(instance.find_method("missing"));
  CHECK(instance.get("missing").is_null());
  CHECK_FALSE(instance.get_ref("missing"));
  CHECK_FALSE(instance.set("missing", 1));
  CHECK_FALSE(instance.call("missing"));
}

TEST_CASE("meta api - const instance is read only", "[meta][api]") {
  const caors::error status =
      caors::registration::type_<meta_api_const_owner>("caors.test.meta.api.const_owner")
          .meta()
          .property("health", &meta_api_const_owner::health)
          .method("add_health", &meta_api_const_owner::add_health)
          .method("read_health", &meta_api_const_owner::read_health)
          .commit();
  CHECK_FALSE(status);

  meta_api_const_owner owner{};
  const meta_api_const_owner& readonly = owner;

  caors::meta::instance instance = caors::meta::view_const(readonly);
  REQUIRE(instance);
  CHECK_FALSE(instance.value().writable());

  const caors::meta::value health = instance.get("health");
  REQUIRE(health.is_i64());
  CHECK(health.as_i64() == 7);

  const caors::any_ref health_ref = instance.get_ref("health");
  REQUIRE(health_ref.valid());
  CHECK_FALSE(health_ref.writable());
  CHECK(health_ref.try_get<int>() != nullptr);
  CHECK(health_ref.try_get_mutable<int>() == nullptr);

  CHECK_FALSE(instance.set("health", 11));
  CHECK(owner.health == 7);

  const caors::meta::invoke_value_result readonly_result = instance.call("read_health");
  REQUIRE(readonly_result.has_value());
  REQUIRE(readonly_result.value().is_i64());
  CHECK(readonly_result.value().as_i64() == 7);

  int delta = 5;
  std::array<caors::argument, 1> arguments{caors::arg(delta)};
  CHECK_FALSE(instance.call_any(
      "add_health", std::span<const caors::argument>{arguments.data(), arguments.size()}));
  CHECK(owner.health == 7);
}
