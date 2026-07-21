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

using rayf = ray_t<float>;
using ray = rayf;
using dray = ray_t<double>;

using spheref = sphere_t<float>;
using sphere = spheref;
using dsphere = sphere_t<double>;

using planef = plane_t<float>;
using plane = planef;
using dplane = plane_t<double>;

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

} // namespace caors::math

#endif
