// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_MATH_GEOMETRY_H_
#define CAORS_MATH_GEOMETRY_H_

#include "caors/math/aabb.h"
#include "caors/math/functions.h"

namespace caors::math {

/** @brief 三维射线模板。 */
template <typename T> struct ray_t {
  using value_type = T;

  vec3_t<T> origin{};
  vec3_t<T> direction{T{}, T{}, T{1}};
};

/** @brief 三维球体模板。 */
template <typename T> struct sphere_t {
  using value_type = T;

  vec3_t<T> center{};
  T radius{};
};

/** @brief 三维平面模板，满足 `dot(normal, point) + distance = 0`。 */
template <typename T> struct plane_t {
  using value_type = T;

  vec3_t<T> normal{T{}, T{1}, T{}};
  T distance{};

  [[nodiscard]] static plane_t from_point_normal(vec3_t<T> point, vec3_t<T> normal) noexcept {
    const vec3_t<T> unit_normal = normalized(normal);
    return {unit_normal, -dot(unit_normal, point)};
  }

  template <typename U>
  [[nodiscard]] constexpr common_scalar_t<T, U> signed_distance(vec3_t<U> point) const noexcept {
    return dot(normal, point) + distance;
  }
};

/** @brief 三维视锥体模板，平面朝向可见区域内部。 */
template <typename T> struct frustum_t {
  using value_type = T;
  using plane_type = plane_t<T>;

  plane_type left{};
  plane_type right{};
  plane_type bottom{};
  plane_type top{};
  plane_type near_plane{};
  plane_type far_plane{};
};

using rayf = ray_t<float>;
using ray = rayf;
using dray = ray_t<double>;

using spheref = sphere_t<float>;
using sphere = spheref;
using dsphere = sphere_t<double>;

using planef = plane_t<float>;
using plane = planef;
using dplane = plane_t<double>;

using frustumf = frustum_t<float>;
using frustum = frustumf;
using dfrustum = frustum_t<double>;

template <typename T, typename U>
[[nodiscard]] constexpr bool operator==(ray_t<T> lhs, ray_t<U> rhs) noexcept {
  return lhs.origin == rhs.origin && lhs.direction == rhs.direction;
}

template <typename T, typename U>
[[nodiscard]] constexpr bool operator==(sphere_t<T> lhs, sphere_t<U> rhs) noexcept {
  return lhs.center == rhs.center && lhs.radius == rhs.radius;
}

template <typename T, typename U>
[[nodiscard]] constexpr bool operator==(plane_t<T> lhs, plane_t<U> rhs) noexcept {
  return lhs.normal == rhs.normal && lhs.distance == rhs.distance;
}

template <typename T, typename U>
[[nodiscard]] constexpr bool operator==(frustum_t<T> lhs, frustum_t<U> rhs) noexcept {
  return lhs.left == rhs.left && lhs.right == rhs.right && lhs.bottom == rhs.bottom &&
         lhs.top == rhs.top && lhs.near_plane == rhs.near_plane && lhs.far_plane == rhs.far_plane;
}

template <typename T> [[nodiscard]] inline plane_t<T> normalized(plane_t<T> value) noexcept {
  const auto normal_length = length(value.normal);
  return normal_length > T{} ? plane_t<T>{value.normal / normal_length,
                                          static_cast<T>(value.distance / normal_length)}
                             : value;
}

template <typename T, typename U>
[[nodiscard]] constexpr bool intersects(aabb_t<T> lhs, aabb_t<U> rhs) noexcept {
  return lhs.min.x <= rhs.max.x && lhs.max.x >= rhs.min.x && lhs.min.y <= rhs.max.y &&
         lhs.max.y >= rhs.min.y && lhs.min.z <= rhs.max.z && lhs.max.z >= rhs.min.z;
}

template <typename T, typename U>
[[nodiscard]] constexpr bool contains(aabb_t<T> bounds, aabb_t<U> other) noexcept {
  return bounds.contains(other.min) && bounds.contains(other.max);
}

template <typename T, typename U>
[[nodiscard]] constexpr aabb_t<common_scalar_t<T, U>> merge(aabb_t<T> lhs, aabb_t<U> rhs) noexcept {
  return {min(lhs.min, rhs.min), max(lhs.max, rhs.max)};
}

template <typename T, typename U>
[[nodiscard]] constexpr aabb_t<common_scalar_t<T, U>> expanded(aabb_t<T> bounds,
                                                               vec3_t<U> point) noexcept {
  return {min(bounds.min, point), max(bounds.max, point)};
}

template <typename T, typename U>
[[nodiscard]] constexpr bool intersects(sphere_t<T> lhs, sphere_t<U> rhs) noexcept {
  const common_scalar_t<T, U> radius = lhs.radius + rhs.radius;
  return length_squared(lhs.center - rhs.center) <= radius * radius;
}

template <typename T, typename U>
[[nodiscard]] constexpr bool intersects(plane_t<T> value, sphere_t<U> bounds) noexcept {
  return abs(value.signed_distance(bounds.center)) <= bounds.radius;
}

template <typename T, typename U>
[[nodiscard]] constexpr bool contains(frustum_t<T> bounds, vec3_t<U> point) noexcept {
  return bounds.left.signed_distance(point) >= common_scalar_t<T, U>{} &&
         bounds.right.signed_distance(point) >= common_scalar_t<T, U>{} &&
         bounds.bottom.signed_distance(point) >= common_scalar_t<T, U>{} &&
         bounds.top.signed_distance(point) >= common_scalar_t<T, U>{} &&
         bounds.near_plane.signed_distance(point) >= common_scalar_t<T, U>{} &&
         bounds.far_plane.signed_distance(point) >= common_scalar_t<T, U>{};
}

template <typename T, typename U>
[[nodiscard]] constexpr bool intersects(frustum_t<T> bounds, sphere_t<U> value) noexcept {
  return bounds.left.signed_distance(value.center) >= -value.radius &&
         bounds.right.signed_distance(value.center) >= -value.radius &&
         bounds.bottom.signed_distance(value.center) >= -value.radius &&
         bounds.top.signed_distance(value.center) >= -value.radius &&
         bounds.near_plane.signed_distance(value.center) >= -value.radius &&
         bounds.far_plane.signed_distance(value.center) >= -value.radius;
}

template <typename T, typename U>
[[nodiscard]] constexpr bool intersects(frustum_t<T> bounds, aabb_t<U> value) noexcept {
  const auto outside = [](plane_t<T> plane, aabb_t<U> box) noexcept {
    const vec3_t<U> positive{
        plane.normal.x >= T{} ? box.max.x : box.min.x,
        plane.normal.y >= T{} ? box.max.y : box.min.y,
        plane.normal.z >= T{} ? box.max.z : box.min.z,
    };
    return plane.signed_distance(positive) < common_scalar_t<T, U>{};
  };

  return !outside(bounds.left, value) && !outside(bounds.right, value) &&
         !outside(bounds.bottom, value) && !outside(bounds.top, value) &&
         !outside(bounds.near_plane, value) && !outside(bounds.far_plane, value);
}

template <typename T, typename U, typename E>
[[nodiscard]] constexpr bool nearly_equal(ray_t<T> lhs, ray_t<U> rhs, E epsilon) noexcept {
  return nearly_equal(lhs.origin, rhs.origin, epsilon) &&
         nearly_equal(lhs.direction, rhs.direction, epsilon);
}

template <typename T, typename U>
[[nodiscard]] constexpr bool nearly_equal(ray_t<T> lhs, ray_t<U> rhs) noexcept {
  using epsilon_type = common_scalar_t<T, U>;
  return nearly_equal(lhs, rhs, static_cast<epsilon_type>(0.00001));
}

template <typename T, typename U, typename E>
[[nodiscard]] constexpr bool nearly_equal(sphere_t<T> lhs, sphere_t<U> rhs, E epsilon) noexcept {
  return nearly_equal(lhs.center, rhs.center, epsilon) &&
         nearly_equal(lhs.radius, rhs.radius, epsilon);
}

template <typename T, typename U>
[[nodiscard]] constexpr bool nearly_equal(sphere_t<T> lhs, sphere_t<U> rhs) noexcept {
  using epsilon_type = common_scalar_t<T, U>;
  return nearly_equal(lhs, rhs, static_cast<epsilon_type>(0.00001));
}

template <typename T, typename U, typename E>
[[nodiscard]] constexpr bool nearly_equal(plane_t<T> lhs, plane_t<U> rhs, E epsilon) noexcept {
  return nearly_equal(lhs.normal, rhs.normal, epsilon) &&
         nearly_equal(lhs.distance, rhs.distance, epsilon);
}

template <typename T, typename U>
[[nodiscard]] constexpr bool nearly_equal(plane_t<T> lhs, plane_t<U> rhs) noexcept {
  using epsilon_type = common_scalar_t<T, U>;
  return nearly_equal(lhs, rhs, static_cast<epsilon_type>(0.00001));
}

template <typename T, typename U, typename E>
[[nodiscard]] constexpr bool nearly_equal(frustum_t<T> lhs, frustum_t<U> rhs, E epsilon) noexcept {
  return nearly_equal(lhs.left, rhs.left, epsilon) && nearly_equal(lhs.right, rhs.right, epsilon) &&
         nearly_equal(lhs.bottom, rhs.bottom, epsilon) && nearly_equal(lhs.top, rhs.top, epsilon) &&
         nearly_equal(lhs.near_plane, rhs.near_plane, epsilon) &&
         nearly_equal(lhs.far_plane, rhs.far_plane, epsilon);
}

template <typename T, typename U>
[[nodiscard]] constexpr bool nearly_equal(frustum_t<T> lhs, frustum_t<U> rhs) noexcept {
  using epsilon_type = common_scalar_t<T, U>;
  return nearly_equal(lhs, rhs, static_cast<epsilon_type>(0.00001));
}

} // namespace caors::math

#endif
