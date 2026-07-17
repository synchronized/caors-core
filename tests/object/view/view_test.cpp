// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta.h"
#include "caors/object.h"

#include <catch2/catch_all.hpp>

namespace {

struct object_view_target : caors::object {};

class object_view_member_owner : public caors::object_base<object_view_member_owner> {
public:
  static constexpr std::string_view object_type_name = "caors.test.object.view_member_owner";

  int health = 7;
  caors::object_ptr<object_view_target> target{};

  int add_health(int delta) {
    health += delta;
    return health;
  }
};

class object_view_pointer_owner : public caors::object_base<object_view_pointer_owner> {
public:
  static constexpr std::string_view object_type_name = "caors.test.object.view_pointer_owner";

  caors::object_ptr<object_view_target> target{};
};

struct object_view_plain_owner {
  int health = 7;
};

} // namespace

TEST_CASE("object - view reads writes and calls reflected object members", "[object]") {
  const caors::error status =
      caors::meta::class_<object_view_member_owner>(object_view_member_owner::object_type_name)
          .property("health", &object_view_member_owner::health)
          .property("target", &object_view_member_owner::target)
          .method("add_health", &object_view_member_owner::add_health)
          .commit();
  CHECK_FALSE(status);

  object_view_member_owner owner{};
  caors::objects::object_view object = caors::objects::view(owner);
  REQUIRE(object);
  CHECK(object.ptr() == &owner);
  CHECK(object.property_count() == 2);
  CHECK(object.method_count() == 1);
  CHECK(object.properties().size() == 2);
  CHECK(object.methods().size() == 1);

  const caors::any health = object.get("health");
  REQUIRE(health.valid());
  REQUIRE(health.try_get<int>() != nullptr);
  CHECK(*health.try_get<int>() == 7);

  CHECK(object.set("health", 11));
  CHECK(owner.health == 11);

  const caors::any result = object.call("add_health", 5);
  REQUIRE(result.valid());
  REQUIRE(result.try_get<int>() != nullptr);
  CHECK(*result.try_get<int>() == 16);
  CHECK(owner.health == 16);

  const caors::any_ref owner_ref = caors::any_ref::from(owner);
  caors::objects::object_view erased_object = caors::objects::view(owner_ref);
  REQUIRE(erased_object);
  CHECK(erased_object.ptr() == nullptr);
  CHECK(erased_object.get("health").try_get<int>() != nullptr);
}

TEST_CASE("object - view reads and writes object pointer fields", "[object]") {
  const caors::error status =
      caors::meta::class_<object_view_pointer_owner>(object_view_pointer_owner::object_type_name)
          .property("target", &object_view_pointer_owner::target)
          .commit();
  CHECK_FALSE(status);

  object_view_pointer_owner owner{};
  object_view_target target{};

  caors::objects::object_view object = caors::objects::view(owner);
  REQUIRE(object);
  CHECK(object.ptr() == &owner);
  CHECK(object.get_ptr("target") == nullptr);
  CHECK(object.set_ptr("target", &target));
  CHECK(owner.target.get() == &target);
  CHECK(object.get_ptr("target") == &target);
}

TEST_CASE("object - view reports non object pointer fields as empty", "[object]") {
  const caors::error status =
      caors::meta::class_<object_view_plain_owner>("caors.test.object.view_plain_owner")
          .property("health", &object_view_plain_owner::health)
          .commit();
  CHECK_FALSE(status);

  object_view_plain_owner owner{};

  caors::objects::object_view object = caors::objects::view(owner);
  REQUIRE(object);
  CHECK(object.ptr() == nullptr);
  CHECK(object.get_ptr("health") == nullptr);
  CHECK_FALSE(object.set_ptr("health", nullptr));
  CHECK(owner.health == 7);
}
