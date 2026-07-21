// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_MATH_AABB_H_
#define CAORS_MATH_AABB_H_

#include "caors/math/vector.h"

namespace caors::math {

/** @brief 三维轴对齐包围盒，使用最小点和最大点表示。 */
struct aabb {
  vec3 min{};
  vec3 max{};

  [[nodiscard]] static constexpr aabb from_center_size(vec3 center, vec3 size) noexcept {
    const vec3 half_size = size * 0.5F;
    return {center - half_size, center + half_size};
  }

  [[nodiscard]] constexpr vec3 size() const noexcept { return max - min; }

  [[nodiscard]] constexpr vec3 center() const noexcept { return (min + max) * 0.5F; }

  [[nodiscard]] constexpr bool contains(vec3 point) const noexcept {
    return point.x >= min.x && point.y >= min.y && point.z >= min.z && point.x <= max.x &&
           point.y <= max.y && point.z <= max.z;
  }
};

[[nodiscard]] constexpr bool operator==(aabb lhs, aabb rhs) noexcept {
  return lhs.min == rhs.min && lhs.max == rhs.max;
}

[[nodiscard]] constexpr bool nearly_equal(aabb lhs, aabb rhs, float epsilon = 0.00001F) noexcept {
  return nearly_equal(lhs.min, rhs.min, epsilon) && nearly_equal(lhs.max, rhs.max, epsilon);
}

} // namespace caors::math

#endif
