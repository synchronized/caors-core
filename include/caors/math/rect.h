// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_MATH_RECT_H_
#define CAORS_MATH_RECT_H_

#include "caors/math/vector.h"

namespace caors::math {

/** @brief 二维矩形，使用左上角位置和尺寸表示。 */
struct rect {
  vec2 position{};
  vec2 size{};

  [[nodiscard]] static constexpr rect from_min_max(vec2 minimum, vec2 maximum) noexcept {
    return {minimum, maximum - minimum};
  }

  [[nodiscard]] constexpr vec2 end() const noexcept { return position + size; }

  [[nodiscard]] constexpr vec2 center() const noexcept { return position + size * 0.5F; }

  [[nodiscard]] constexpr bool contains(vec2 point) const noexcept {
    const vec2 maximum = end();
    return point.x >= position.x && point.y >= position.y && point.x <= maximum.x &&
           point.y <= maximum.y;
  }
};

[[nodiscard]] constexpr bool operator==(rect lhs, rect rhs) noexcept {
  return lhs.position == rhs.position && lhs.size == rhs.size;
}

[[nodiscard]] constexpr bool nearly_equal(rect lhs, rect rhs, float epsilon = 0.00001F) noexcept {
  return nearly_equal(lhs.position, rhs.position, epsilon) &&
         nearly_equal(lhs.size, rhs.size, epsilon);
}

} // namespace caors::math

#endif
