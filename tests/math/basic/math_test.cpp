// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/math.h"

#include <catch2/catch_all.hpp>

namespace {

[[nodiscard]] float ndc_z(caors::math::vec4 value) noexcept { return value.z / value.w; }

} // namespace

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

TEST_CASE("math - vector and matrix aliases cover common scalar types", "[math]") {
  const caors::math::dvec2 precise{1.5, 2.5};
  CHECK(caors::math::dot(precise, caors::math::dvec2{2.0, 4.0}) == Catch::Approx(13.0));
  CHECK(caors::math::length(caors::math::dvec2{6.0, 8.0}) == Catch::Approx(10.0));

  const caors::math::ivec2 grid{2, 4};
  CHECK(grid + caors::math::ivec2{3, 5} == caors::math::ivec2{5, 9});
  CHECK(grid * 2 == caors::math::ivec2{4, 8});

  const caors::math::irect pixels{{10, 20}, {128, 64}};
  CHECK(pixels.contains(caors::math::ivec2{138, 84}));
  CHECK_FALSE(pixels.contains(caors::math::ivec2{139, 84}));

  const caors::math::dmat4 identity = caors::math::dmat4::identity();
  CHECK(identity * caors::math::dvec4{1.0, 2.0, 3.0, 1.0} ==
        caors::math::dvec4{1.0, 2.0, 3.0, 1.0});
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
  constexpr float near_z = 1.0F;
  constexpr float far_z = 10.0F;

  const caors::math::mat4 rh_zo =
      caors::math::perspective_rh_zo(caors::math::half_pi, 1.0F, near_z, far_z);
  CHECK(ndc_z(rh_zo * caors::math::vec4{0.0F, 0.0F, -near_z, 1.0F}) == Catch::Approx(0.0F));
  CHECK(ndc_z(rh_zo * caors::math::vec4{0.0F, 0.0F, -far_z, 1.0F}) == Catch::Approx(1.0F));

  const caors::math::mat4 rh_no =
      caors::math::perspective_rh_no(caors::math::half_pi, 1.0F, near_z, far_z);
  CHECK(ndc_z(rh_no * caors::math::vec4{0.0F, 0.0F, -near_z, 1.0F}) == Catch::Approx(-1.0F));
  CHECK(ndc_z(rh_no * caors::math::vec4{0.0F, 0.0F, -far_z, 1.0F}) == Catch::Approx(1.0F));

  const caors::math::mat4 lh_zo =
      caors::math::perspective_lh_zo(caors::math::half_pi, 1.0F, near_z, far_z);
  CHECK(ndc_z(lh_zo * caors::math::vec4{0.0F, 0.0F, near_z, 1.0F}) == Catch::Approx(0.0F));
  CHECK(ndc_z(lh_zo * caors::math::vec4{0.0F, 0.0F, far_z, 1.0F}) == Catch::Approx(1.0F));

  const caors::math::mat4 lh_no =
      caors::math::perspective_lh_no(caors::math::half_pi, 1.0F, near_z, far_z);
  CHECK(ndc_z(lh_no * caors::math::vec4{0.0F, 0.0F, near_z, 1.0F}) == Catch::Approx(-1.0F));
  CHECK(ndc_z(lh_no * caors::math::vec4{0.0F, 0.0F, far_z, 1.0F}) == Catch::Approx(1.0F));
}

TEST_CASE("math - orthographic helpers map depth by handedness and clip range", "[math]") {
  constexpr float near_z = 1.0F;
  constexpr float far_z = 10.0F;

  const caors::math::mat4 rh_zo =
      caors::math::orthographic_rh_zo(-1.0F, 1.0F, -1.0F, 1.0F, near_z, far_z);
  CHECK((rh_zo * caors::math::vec4{0.0F, 0.0F, -near_z, 1.0F}).z == Catch::Approx(0.0F));
  CHECK((rh_zo * caors::math::vec4{0.0F, 0.0F, -far_z, 1.0F}).z == Catch::Approx(1.0F));

  const caors::math::mat4 rh_no =
      caors::math::orthographic_rh_no(-1.0F, 1.0F, -1.0F, 1.0F, near_z, far_z);
  CHECK((rh_no * caors::math::vec4{0.0F, 0.0F, -near_z, 1.0F}).z == Catch::Approx(-1.0F));
  CHECK((rh_no * caors::math::vec4{0.0F, 0.0F, -far_z, 1.0F}).z == Catch::Approx(1.0F));

  const caors::math::mat4 lh_zo =
      caors::math::orthographic_lh_zo(-1.0F, 1.0F, -1.0F, 1.0F, near_z, far_z);
  CHECK((lh_zo * caors::math::vec4{0.0F, 0.0F, near_z, 1.0F}).z == Catch::Approx(0.0F));
  CHECK((lh_zo * caors::math::vec4{0.0F, 0.0F, far_z, 1.0F}).z == Catch::Approx(1.0F));

  const caors::math::mat4 lh_no =
      caors::math::orthographic_lh_no(-1.0F, 1.0F, -1.0F, 1.0F, near_z, far_z);
  CHECK((lh_no * caors::math::vec4{0.0F, 0.0F, near_z, 1.0F}).z == Catch::Approx(-1.0F));
  CHECK((lh_no * caors::math::vec4{0.0F, 0.0F, far_z, 1.0F}).z == Catch::Approx(1.0F));
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

  const caors::math::vec3 eye{1.0F, 2.0F, 3.0F};
  const caors::math::mat4 moved_right = caors::math::look_at_rh(
      eye, caors::math::vec3{1.0F, 2.0F, 2.0F}, caors::math::vec3{0.0F, 1.0F, 0.0F});
  const caors::math::mat4 moved_left = caors::math::look_at_lh(
      eye, caors::math::vec3{1.0F, 2.0F, 4.0F}, caors::math::vec3{0.0F, 1.0F, 0.0F});

  CHECK(caors::math::nearly_equal(caors::math::transform_point(moved_right, eye),
                                  caors::math::vec3::zero()));
  CHECK(caors::math::nearly_equal(
      caors::math::transform_point(moved_right, caors::math::vec3{1.0F, 2.0F, 2.0F}),
      caors::math::vec3{0.0F, 0.0F, -1.0F}));
  CHECK(caors::math::nearly_equal(caors::math::transform_point(moved_left, eye),
                                  caors::math::vec3::zero()));
  CHECK(caors::math::nearly_equal(
      caors::math::transform_point(moved_left, caors::math::vec3{1.0F, 2.0F, 4.0F}),
      caors::math::vec3{0.0F, 0.0F, 1.0F}));
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

  const caors::math::dplane precise_ground = caors::math::dplane::from_point_normal(
      caors::math::dvec3::zero(), caors::math::dvec3{0.0, 1.0, 0.0});
  CHECK(precise_ground.signed_distance(caors::math::dvec3{0.0, 2.5, 0.0}) == Catch::Approx(2.5));
  CHECK(precise_ground.signed_distance(caors::math::vec3{0.0F, 1.5F, 0.0F}) == Catch::Approx(1.5));
  CHECK(caors::math::intersects(precise_ground, caors::math::dsphere{{0.0, 0.5, 0.0}, 1.0}));

  const caors::math::dray ray{{1.0, 2.0, 3.0}, {0.0, 0.0, -1.0}};
  CHECK(ray.origin == caors::math::dvec3{1.0, 2.0, 3.0});
  CHECK(ray.direction == caors::math::dvec3{0.0, 0.0, -1.0});
}

TEST_CASE("math - traits expose scalar and component access", "[math]") {
  caors::math::vec3 value{1.0F, 2.0F, 3.0F};
  CHECK(caors::math::vector_traits<caors::math::vec3>::size == 3U);
  CHECK(caors::math::vector_traits<caors::math::vec3>::get(value, 1U) == 2.0F);
  caors::math::vector_traits<caors::math::vec3>::get(value, 2U) = 4.0F;
  CHECK(value.z == 4.0F);

  caors::math::mat4 matrix = caors::math::mat4::identity();
  CHECK(caors::math::matrix_traits<caors::math::mat4>::rows == 4U);
  CHECK(caors::math::matrix_traits<caors::math::mat4>::columns == 4U);
  caors::math::matrix_traits<caors::math::mat4>::get(matrix, 0U, 3U) = 12.0F;
  CHECK(matrix(0, 3) == 12.0F);

  caors::math::ray ray{};
  caors::math::ray_traits<caors::math::ray>::origin(ray) = caors::math::vec3{1.0F, 2.0F, 3.0F};
  CHECK(ray.origin == caors::math::vec3{1.0F, 2.0F, 3.0F});
  const caors::math::ray const_ray = ray;
  CHECK(caors::math::ray_traits<caors::math::ray>::origin(const_ray) ==
        caors::math::vec3{1.0F, 2.0F, 3.0F});

  const caors::math::sphere sphere{{1.0F, 2.0F, 3.0F}, 4.0F};
  CHECK(caors::math::sphere_traits<caors::math::sphere>::radius(sphere) == 4.0F);

  const caors::math::plane plane{{0.0F, 1.0F, 0.0F}, 2.0F};
  CHECK(caors::math::plane_traits<caors::math::plane>::distance(plane) == 2.0F);
}
