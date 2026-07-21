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

TEST_CASE("math - helpers provide common scalar and vector utilities", "[math]") {
  CHECK(caors::math::radians(180.0F) == Catch::Approx(caors::math::pi));
  CHECK(caors::math::degrees(caors::math::half_pi) == Catch::Approx(90.0F));
  CHECK(caors::math::clamp(2.0F, 0.0F, 1.0F) == Catch::Approx(1.0F));
  CHECK(caors::math::lerp(10.0F, 20.0F, 0.25F) == Catch::Approx(12.5F));
  CHECK(caors::math::smoothstep(0.0F, 1.0F, 0.5F) == Catch::Approx(0.5F));
  CHECK(
      caors::math::min(caors::math::vec3{2.0F, 4.0F, 1.0F}, caors::math::vec3{1.0F, 5.0F, 3.0F}) ==
      caors::math::vec3{1.0F, 4.0F, 1.0F});
}

TEST_CASE("math - transforms use row-major storage and column-vector semantics", "[math]") {
  const caors::math::mat4 transform = caors::math::trs(
      caors::math::vec3{10.0F, 20.0F, 30.0F},
      caors::math::quat::from_axis_angle(caors::math::vec3{0.0F, 0.0F, 1.0F}, caors::math::half_pi),
      caors::math::vec3{2.0F, 2.0F, 2.0F});

  const caors::math::vec3 point =
      caors::math::transform_point(transform, caors::math::vec3{1.0F, 0.0F, 0.0F});
  const caors::math::vec3 vector =
      caors::math::transform_vector(transform, caors::math::vec3{1.0F, 0.0F, 0.0F});

  CHECK(caors::math::nearly_equal(point, caors::math::vec3{10.0F, 22.0F, 30.0F}));
  CHECK(caors::math::nearly_equal(vector, caors::math::vec3{0.0F, 2.0F, 0.0F}));
  CHECK(caors::math::nearly_equal(
      caors::math::transform_direction(transform, caors::math::vec3{1.0F, 0.0F, 0.0F}),
      caors::math::vec3{0.0F, 1.0F, 0.0F}));
}

TEST_CASE("math - projection templates map depth by handedness and clip range", "[math]") {
  const caors::math::mat4 rh_zo =
      caors::math::perspective_rh_zo(caors::math::half_pi, 1.0F, 1.0F, 10.0F);
  const caors::math::vec4 rh_near = rh_zo * caors::math::vec4{0.0F, 0.0F, -1.0F, 1.0F};
  const caors::math::vec4 rh_far = rh_zo * caors::math::vec4{0.0F, 0.0F, -10.0F, 1.0F};
  CHECK((rh_near.z / rh_near.w) == Catch::Approx(0.0F));
  CHECK((rh_far.z / rh_far.w) == Catch::Approx(1.0F));

  const caors::math::mat4 lh_no =
      caors::math::perspective_lh_no(caors::math::half_pi, 1.0F, 1.0F, 10.0F);
  const caors::math::vec4 lh_near = lh_no * caors::math::vec4{0.0F, 0.0F, 1.0F, 1.0F};
  const caors::math::vec4 lh_far = lh_no * caors::math::vec4{0.0F, 0.0F, 10.0F, 1.0F};
  CHECK((lh_near.z / lh_near.w) == Catch::Approx(-1.0F));
  CHECK((lh_far.z / lh_far.w) == Catch::Approx(1.0F));

  const caors::math::mat4 ortho = caors::math::orthographic<caors::math::handedness::right,
                                                            caors::math::clip_depth::zero_to_one>(
      -1.0F, 1.0F, -1.0F, 1.0F, 1.0F, 10.0F);
  const caors::math::vec4 ortho_near = ortho * caors::math::vec4{0.0F, 0.0F, -1.0F, 1.0F};
  const caors::math::vec4 ortho_far = ortho * caors::math::vec4{0.0F, 0.0F, -10.0F, 1.0F};
  CHECK(ortho_near.z == Catch::Approx(0.0F));
  CHECK(ortho_far.z == Catch::Approx(1.0F));
}

TEST_CASE("math - look-at helpers encode right and left handed view spaces", "[math]") {
  const caors::math::mat4 right =
      caors::math::look_at_rh(caors::math::vec3::zero(), caors::math::vec3{0.0F, 0.0F, -1.0F},
                              caors::math::vec3{0.0F, 1.0F, 0.0F});
  const caors::math::mat4 left =
      caors::math::look_at_lh(caors::math::vec3::zero(), caors::math::vec3{0.0F, 0.0F, 1.0F},
                              caors::math::vec3{0.0F, 1.0F, 0.0F});

  CHECK(right == caors::math::mat4::identity());
  CHECK(left == caors::math::mat4::identity());
}

TEST_CASE("math - geometry helpers cover common bounds operations", "[math]") {
  const caors::math::aabb left{{0.0F, 0.0F, 0.0F}, {2.0F, 2.0F, 2.0F}};
  const caors::math::aabb right{{1.0F, 1.0F, 1.0F}, {3.0F, 3.0F, 3.0F}};
  const caors::math::aabb outside{{4.0F, 4.0F, 4.0F}, {5.0F, 5.0F, 5.0F}};

  CHECK(caors::math::intersects(left, right));
  CHECK_FALSE(caors::math::intersects(left, outside));
  CHECK(caors::math::merge(left, outside) ==
        caors::math::aabb{{0.0F, 0.0F, 0.0F}, {5.0F, 5.0F, 5.0F}});
  CHECK(caors::math::expanded(left, caors::math::vec3{-1.0F, 1.0F, 4.0F}) ==
        caors::math::aabb{{-1.0F, 0.0F, 0.0F}, {2.0F, 2.0F, 4.0F}});

  const caors::math::plane ground = caors::math::plane::from_point_normal(
      caors::math::vec3::zero(), caors::math::vec3{0.0F, 1.0F, 0.0F});
  CHECK(ground.signed_distance(caors::math::vec3{0.0F, 2.0F, 0.0F}) == Catch::Approx(2.0F));
  CHECK(caors::math::intersects(ground, caors::math::sphere{{0.0F, 0.5F, 0.0F}, 1.0F}));
}
