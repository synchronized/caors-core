// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/math.h"

#include <catch2/catch_all.hpp>

TEST_CASE("math - vector operations provide common arithmetic", "[math]") {
  const caors::math::vec3 left{1.0F, 2.0F, 3.0F};
  const caors::math::vec3 right{4.0F, 5.0F, 6.0F};

  CHECK(left + right == caors::math::vec3{5.0F, 7.0F, 9.0F});
  CHECK(right - left == caors::math::vec3{3.0F, 3.0F, 3.0F});
  CHECK(left * 2.0F == caors::math::vec3{2.0F, 4.0F, 6.0F});
  CHECK(caors::math::dot(left, right) == Catch::Approx(32.0F));
  CHECK(caors::math::cross(left, right) == caors::math::vec3{-3.0F, 6.0F, -3.0F});
  CHECK(caors::math::length(caors::math::vec2{3.0F, 4.0F}) == Catch::Approx(5.0F));
  CHECK(caors::math::nearly_equal(caors::math::normalized(caors::math::vec3{0.0F, 3.0F, 4.0F}),
                                  caors::math::vec3{0.0F, 0.6F, 0.8F}));
}

TEST_CASE("math - quaternion rotates vectors", "[math]") {
  constexpr float half_pi = 1.57079632679F;
  const caors::math::quat rotation =
      caors::math::quat::from_axis_angle(caors::math::vec3{0.0F, 0.0F, 1.0F}, half_pi);

  const caors::math::vec3 rotated =
      caors::math::rotate(rotation, caors::math::vec3{1.0F, 0.0F, 0.0F});

  CHECK(caors::math::nearly_equal(rotated, caors::math::vec3{0.0F, 1.0F, 0.0F}));
}

TEST_CASE("math - matrix identity preserves vectors", "[math]") {
  const caors::math::mat3 identity3 = caors::math::mat3::identity();
  const caors::math::mat4 identity4 = caors::math::mat4::identity();

  CHECK(identity3 * caors::math::vec3{1.0F, 2.0F, 3.0F} == caors::math::vec3{1.0F, 2.0F, 3.0F});
  CHECK(identity4 * caors::math::vec4{1.0F, 2.0F, 3.0F, 1.0F} ==
        caors::math::vec4{1.0F, 2.0F, 3.0F, 1.0F});
  CHECK(identity4 * identity4 == identity4);
}

TEST_CASE("math - rect and aabb expose containment helpers", "[math]") {
  const caors::math::rect rectangle{{10.0F, 20.0F}, {100.0F, 50.0F}};
  CHECK(rectangle.contains(caors::math::vec2{10.0F, 20.0F}));
  CHECK(rectangle.contains(caors::math::vec2{110.0F, 70.0F}));
  CHECK_FALSE(rectangle.contains(caors::math::vec2{111.0F, 70.0F}));
  CHECK(rectangle.center() == caors::math::vec2{60.0F, 45.0F});

  const caors::math::aabb bounds = caors::math::aabb::from_center_size(
      caors::math::vec3{0.0F, 0.0F, 0.0F}, caors::math::vec3{2.0F, 4.0F, 6.0F});
  CHECK(bounds.min == caors::math::vec3{-1.0F, -2.0F, -3.0F});
  CHECK(bounds.max == caors::math::vec3{1.0F, 2.0F, 3.0F});
  CHECK(bounds.contains(caors::math::vec3{1.0F, 0.0F, 0.0F}));
  CHECK_FALSE(bounds.contains(caors::math::vec3{1.1F, 0.0F, 0.0F}));
}
