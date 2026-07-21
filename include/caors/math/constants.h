// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_MATH_CONSTANTS_H_
#define CAORS_MATH_CONSTANTS_H_

namespace caors::math {

template <typename T>
inline constexpr T pi_v = static_cast<T>(3.141592653589793238462643383279502884);

/** @brief 单精度圆周率。 */
inline constexpr float pi = pi_v<float>;

/** @brief 单精度半圆周率。 */
inline constexpr float half_pi = pi * 0.5F;

/** @brief 单精度二倍圆周率。 */
inline constexpr float two_pi = pi * 2.0F;

/** @brief 角度转弧度。 */
template <typename T> [[nodiscard]] constexpr T radians(T degrees) noexcept {
  return degrees * (pi_v<T> / T{180});
}

/** @brief 弧度转角度。 */
template <typename T> [[nodiscard]] constexpr T degrees(T radians) noexcept {
  return radians * (T{180} / pi_v<T>);
}

} // namespace caors::math

#endif
