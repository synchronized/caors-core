// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/math/registration.h"
#include "caors/meta.h"

#include <catch2/catch_all.hpp>

TEST_CASE("math - meta registration helper registers built in math types", "[math][meta]") {
  REQUIRE(caors::math::register_meta_types().succeeded());
  REQUIRE(caors::math::register_meta_types().succeeded());

  const caors::meta::type vec3_type = caors::meta::find<caors::math::vec3>();
  REQUIRE(vec3_type);
  CHECK(vec3_type.name() == "caors.math.vec3");
  CHECK(vec3_type.property_count() == 3);

  const caors::meta::property x = vec3_type.find_property("x");
  const caors::meta::property y = vec3_type.find_property("y");
  const caors::meta::property z = vec3_type.find_property("z");
  REQUIRE(x);
  REQUIRE(y);
  REQUIRE(z);
  CHECK(x.get_type().name() == "float");
  CHECK(y.get_type().name() == "float");
  CHECK(z.get_type().name() == "float");

  caors::math::vec3 value{1.0F, 2.0F, 3.0F};
  REQUIRE(x.try_get<float>(caors::meta::view(value)).has_value());
  CHECK(*x.try_get<float>(caors::meta::view(value)) == 1.0F);
  CHECK(x.try_set<float>(caors::meta::view(value), 4.0F));
  CHECK(value.x == 4.0F);

  const caors::meta::type quat_type = caors::meta::find<caors::math::quat>();
  REQUIRE(quat_type);
  CHECK(quat_type.find_property("w"));

  const caors::meta::type rect_type = caors::meta::find<caors::math::rect>();
  REQUIRE(rect_type);
  const caors::meta::property position = rect_type.find_property("position");
  REQUIRE(position);
  CHECK(position.get_type().id() == caors::meta::id_of<caors::math::vec2>());

  const caors::meta::type aabb_type = caors::meta::find<caors::math::aabb>();
  REQUIRE(aabb_type);
  const caors::meta::property minimum = aabb_type.find_property("min");
  REQUIRE(minimum);
  CHECK(minimum.get_type().id() == caors::meta::id_of<caors::math::vec3>());

  const caors::meta::type mat4_type = caors::meta::find<caors::math::mat4>();
  REQUIRE(mat4_type);
  CHECK(mat4_type.name() == "caors.math.mat4");
  CHECK(mat4_type.property_count() == 0);
}
