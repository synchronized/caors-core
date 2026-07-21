// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_MATH_RECT_H_
#define CAORS_MATH_RECT_H_

#include "caors/math/vector.h"

#include <cstdint>

namespace caors::math {

/** @brief 二维矩形模板，使用左上角位置和尺寸表示。 */
template <typename T> struct rect_t {
  using value_type = T;

  vec2_t<T> position{};
  vec2_t<T> size{};

  [[nodiscard]] static constexpr rect_t from_min_max(vec2_t<T> minimum,
                                                     vec2_t<T> maximum) noexcept {
    return {minimum, maximum - minimum};
  }

  [[nodiscard]] constexpr vec2_t<T> end() const noexcept { return position + size; }

  [[nodiscard]] constexpr vec2_t<T> center() const noexcept { return position + size / T{2}; }

  [[nodiscard]] constexpr bool contains(vec2_t<T> point) const noexcept {
    const vec2_t<T> maximum = end();
    return point.x >= position.x && point.y >= position.y && point.x <= maximum.x &&
           point.y <= maximum.y;
  }
};

using rectf = rect_t<float>;
using rect = rectf;
using drect = rect_t<double>;
using irect = rect_t<std::int32_t>;

template <typename T, typename U>
[[nodiscard]] constexpr bool operator==(rect_t<T> lhs, rect_t<U> rhs) noexcept {
  return lhs.position == rhs.position && lhs.size == rhs.size;
}

template <typename T, typename U, typename E>
[[nodiscard]] constexpr bool nearly_equal(rect_t<T> lhs, rect_t<U> rhs, E epsilon) noexcept {
  return nearly_equal(lhs.position, rhs.position, epsilon) &&
         nearly_equal(lhs.size, rhs.size, epsilon);
}

template <typename T, typename U>
[[nodiscard]] constexpr bool nearly_equal(rect_t<T> lhs, rect_t<U> rhs) noexcept {
  using epsilon_type = common_scalar_t<T, U>;
  return nearly_equal(lhs, rhs, static_cast<epsilon_type>(0.00001));
}

} // namespace caors::math

#endif
