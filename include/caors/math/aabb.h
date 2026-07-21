// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_MATH_AABB_H_
#define CAORS_MATH_AABB_H_

#include "caors/math/vector.h"

#include <cstdint>

namespace caors::math {

/** @brief 三维轴对齐包围盒模板，使用最小点和最大点表示。 */
template <typename T> struct aabb_t {
  using value_type = T;

  vec3_t<T> min{};
  vec3_t<T> max{};

  [[nodiscard]] static constexpr aabb_t from_center_size(vec3_t<T> center,
                                                         vec3_t<T> size) noexcept {
    const vec3_t<T> half_size = size / T{2};
    return {center - half_size, center + half_size};
  }

  [[nodiscard]] constexpr vec3_t<T> size() const noexcept { return max - min; }

  [[nodiscard]] constexpr vec3_t<T> center() const noexcept { return (min + max) / T{2}; }

  [[nodiscard]] constexpr bool contains(vec3_t<T> point) const noexcept {
    return point.x >= min.x && point.y >= min.y && point.z >= min.z && point.x <= max.x &&
           point.y <= max.y && point.z <= max.z;
  }
};

using aabbf = aabb_t<float>;
using aabb = aabbf;
using daabb = aabb_t<double>;
using iaabb = aabb_t<std::int32_t>;

template <typename T, typename U>
[[nodiscard]] constexpr bool operator==(aabb_t<T> lhs, aabb_t<U> rhs) noexcept {
  return lhs.min == rhs.min && lhs.max == rhs.max;
}

template <typename T, typename U, typename E>
[[nodiscard]] constexpr bool nearly_equal(aabb_t<T> lhs, aabb_t<U> rhs, E epsilon) noexcept {
  return nearly_equal(lhs.min, rhs.min, epsilon) && nearly_equal(lhs.max, rhs.max, epsilon);
}

template <typename T, typename U>
[[nodiscard]] constexpr bool nearly_equal(aabb_t<T> lhs, aabb_t<U> rhs) noexcept {
  using epsilon_type = common_scalar_t<T, U>;
  return nearly_equal(lhs, rhs, static_cast<epsilon_type>(0.00001));
}

} // namespace caors::math

#endif
