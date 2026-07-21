// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_MATH_GEOMETRY_H_
#define CAORS_MATH_GEOMETRY_H_

#include "caors/math/aabb.h"
#include "caors/math/functions.h"

namespace caors::math {

/** @brief 三维射线。 */
struct ray {
  vec3 origin{};
  vec3 direction{0.0F, 0.0F, 1.0F};
};

/** @brief 三维球体。 */
struct sphere {
  vec3 center{};
  float radius = 0.0F;
};

/** @brief 三维平面，满足 `dot(normal, point) + distance = 0`。 */
struct plane {
  vec3 normal{0.0F, 1.0F, 0.0F};
  float distance = 0.0F;

  [[nodiscard]] static plane from_point_normal(vec3 point, vec3 normal) noexcept {
    const vec3 unit_normal = normalized(normal);
    return {unit_normal, -dot(unit_normal, point)};
  }

  [[nodiscard]] constexpr float signed_distance(vec3 point) const noexcept {
    return dot(normal, point) + distance;
  }
};

[[nodiscard]] constexpr bool intersects(aabb lhs, aabb rhs) noexcept {
  return lhs.min.x <= rhs.max.x && lhs.max.x >= rhs.min.x && lhs.min.y <= rhs.max.y &&
         lhs.max.y >= rhs.min.y && lhs.min.z <= rhs.max.z && lhs.max.z >= rhs.min.z;
}

[[nodiscard]] constexpr bool contains(aabb bounds, aabb other) noexcept {
  return bounds.contains(other.min) && bounds.contains(other.max);
}

[[nodiscard]] constexpr aabb merge(aabb lhs, aabb rhs) noexcept {
  return {min(lhs.min, rhs.min), max(lhs.max, rhs.max)};
}

[[nodiscard]] constexpr aabb expanded(aabb bounds, vec3 point) noexcept {
  return {min(bounds.min, point), max(bounds.max, point)};
}

[[nodiscard]] constexpr bool intersects(sphere lhs, sphere rhs) noexcept {
  const float radius = lhs.radius + rhs.radius;
  return length_squared(lhs.center - rhs.center) <= radius * radius;
}

[[nodiscard]] constexpr bool intersects(plane value, sphere bounds) noexcept {
  return abs(value.signed_distance(bounds.center)) <= bounds.radius;
}

} // namespace caors::math

#endif
