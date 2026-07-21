// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_MATH_QUATERNION_H_
#define CAORS_MATH_QUATERNION_H_

#include "caors/math/vector.h"

#include <cmath>

namespace caors::math {

/** @brief 四元数模板，使用 `(x, y, z, w)` 存储，其中 `w` 是标量部分。 */
template <typename T> struct quat_t {
  using value_type = T;

  T x{};
  T y{};
  T z{};
  T w = T{1};

  [[nodiscard]] static constexpr quat_t identity() noexcept { return {}; }

  [[nodiscard]] static inline quat_t from_axis_angle(vec3_t<T> axis, T radians) noexcept {
    const vec3_t<T> unit_axis = normalized(axis);
    const T half_angle = radians * T{0.5};
    const T sine = static_cast<T>(std::sin(half_angle));
    return {
        unit_axis.x * sine,
        unit_axis.y * sine,
        unit_axis.z * sine,
        static_cast<T>(std::cos(half_angle)),
    };
  }
};

using quatf = quat_t<float>;
using quat = quatf;
using dquat = quat_t<double>;

template <typename T, typename U>
[[nodiscard]] constexpr bool operator==(quat_t<T> lhs, quat_t<U> rhs) noexcept {
  return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z && lhs.w == rhs.w;
}

template <typename T, typename U>
[[nodiscard]] constexpr quat_t<common_scalar_t<T, U>> operator*(quat_t<T> lhs,
                                                                quat_t<U> rhs) noexcept {
  return {
      lhs.w * rhs.x + lhs.x * rhs.w + lhs.y * rhs.z - lhs.z * rhs.y,
      lhs.w * rhs.y - lhs.x * rhs.z + lhs.y * rhs.w + lhs.z * rhs.x,
      lhs.w * rhs.z + lhs.x * rhs.y - lhs.y * rhs.x + lhs.z * rhs.w,
      lhs.w * rhs.w - lhs.x * rhs.x - lhs.y * rhs.y - lhs.z * rhs.z,
  };
}

template <typename T> [[nodiscard]] constexpr quat_t<T> conjugate(quat_t<T> value) noexcept {
  return {-value.x, -value.y, -value.z, value.w};
}

template <typename T, typename U>
[[nodiscard]] constexpr common_scalar_t<T, U> dot(quat_t<T> lhs, quat_t<U> rhs) noexcept {
  return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z + lhs.w * rhs.w;
}

template <typename T> [[nodiscard]] constexpr auto length_squared(quat_t<T> value) noexcept {
  return dot(value, value);
}

template <typename T> [[nodiscard]] inline auto length(quat_t<T> value) noexcept {
  return std::sqrt(length_squared(value));
}

template <typename T> [[nodiscard]] inline quat_t<T> normalized(quat_t<T> value) noexcept {
  const auto value_length = length(value);
  if (value_length <= decltype(value_length){}) {
    return quat_t<T>::identity();
  }

  return {
      static_cast<T>(value.x / value_length),
      static_cast<T>(value.y / value_length),
      static_cast<T>(value.z / value_length),
      static_cast<T>(value.w / value_length),
  };
}

template <typename T>
[[nodiscard]] inline vec3_t<T> rotate(quat_t<T> rotation, vec3_t<T> value) noexcept {
  const quat_t<T> unit_rotation = normalized(rotation);
  const quat_t<T> vector_quat{value.x, value.y, value.z, T{}};
  const quat_t<T> rotated = unit_rotation * vector_quat * conjugate(unit_rotation);
  return {rotated.x, rotated.y, rotated.z};
}

template <typename T, typename U, typename E>
[[nodiscard]] constexpr bool nearly_equal(quat_t<T> lhs, quat_t<U> rhs, E epsilon) noexcept {
  return nearly_equal(lhs.x, rhs.x, epsilon) && nearly_equal(lhs.y, rhs.y, epsilon) &&
         nearly_equal(lhs.z, rhs.z, epsilon) && nearly_equal(lhs.w, rhs.w, epsilon);
}

template <typename T, typename U>
[[nodiscard]] constexpr bool nearly_equal(quat_t<T> lhs, quat_t<U> rhs) noexcept {
  using epsilon_type = common_scalar_t<T, U>;
  return nearly_equal(lhs, rhs, static_cast<epsilon_type>(0.00001));
}

} // namespace caors::math

#endif
