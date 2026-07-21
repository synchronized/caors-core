// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_MATH_QUATERNION_H_
#define CAORS_MATH_QUATERNION_H_

#include "caors/math/vector.h"

#include <cmath>

namespace caors::math {

/** @brief 单精度四元数，使用 `(x, y, z, w)` 存储，其中 `w` 是标量部分。 */
struct quat {
  float x = 0.0F;
  float y = 0.0F;
  float z = 0.0F;
  float w = 1.0F;

  [[nodiscard]] static constexpr quat identity() noexcept { return {}; }

  [[nodiscard]] static inline quat from_axis_angle(vec3 axis, float radians) noexcept {
    const vec3 unit_axis = normalized(axis);
    const float half_angle = radians * 0.5F;
    const float sine = std::sin(half_angle);
    return {
        unit_axis.x * sine,
        unit_axis.y * sine,
        unit_axis.z * sine,
        std::cos(half_angle),
    };
  }
};

[[nodiscard]] constexpr bool operator==(quat lhs, quat rhs) noexcept {
  return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z && lhs.w == rhs.w;
}

[[nodiscard]] constexpr quat operator*(quat lhs, quat rhs) noexcept {
  return {
      lhs.w * rhs.x + lhs.x * rhs.w + lhs.y * rhs.z - lhs.z * rhs.y,
      lhs.w * rhs.y - lhs.x * rhs.z + lhs.y * rhs.w + lhs.z * rhs.x,
      lhs.w * rhs.z + lhs.x * rhs.y - lhs.y * rhs.x + lhs.z * rhs.w,
      lhs.w * rhs.w - lhs.x * rhs.x - lhs.y * rhs.y - lhs.z * rhs.z,
  };
}

[[nodiscard]] constexpr quat conjugate(quat value) noexcept {
  return {-value.x, -value.y, -value.z, value.w};
}

[[nodiscard]] constexpr float dot(quat lhs, quat rhs) noexcept {
  return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z + lhs.w * rhs.w;
}

[[nodiscard]] constexpr float length_squared(quat value) noexcept { return dot(value, value); }

[[nodiscard]] inline float length(quat value) noexcept { return std::sqrt(length_squared(value)); }

[[nodiscard]] inline quat normalized(quat value) noexcept {
  const float value_length = length(value);
  if (value_length <= 0.0F) {
    return quat::identity();
  }

  return {
      value.x / value_length,
      value.y / value_length,
      value.z / value_length,
      value.w / value_length,
  };
}

[[nodiscard]] inline vec3 rotate(quat rotation, vec3 value) noexcept {
  const quat unit_rotation = normalized(rotation);
  const quat vector_quat{value.x, value.y, value.z, 0.0F};
  const quat rotated = unit_rotation * vector_quat * conjugate(unit_rotation);
  return {rotated.x, rotated.y, rotated.z};
}

[[nodiscard]] constexpr bool nearly_equal(quat lhs, quat rhs, float epsilon = 0.00001F) noexcept {
  return nearly_equal(lhs.x, rhs.x, epsilon) && nearly_equal(lhs.y, rhs.y, epsilon) &&
         nearly_equal(lhs.z, rhs.z, epsilon) && nearly_equal(lhs.w, rhs.w, epsilon);
}

} // namespace caors::math

#endif
