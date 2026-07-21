// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_MATH_CONSTANTS_H_
#define CAORS_MATH_CONSTANTS_H_

namespace caors::math {

/** @brief 单精度圆周率。 */
inline constexpr float pi = 3.14159265358979323846F;

/** @brief 单精度半圆周率。 */
inline constexpr float half_pi = pi * 0.5F;

/** @brief 单精度二倍圆周率。 */
inline constexpr float two_pi = pi * 2.0F;

/** @brief 角度转弧度。 */
[[nodiscard]] constexpr float radians(float degrees) noexcept { return degrees * (pi / 180.0F); }

/** @brief 弧度转角度。 */
[[nodiscard]] constexpr float degrees(float radians) noexcept { return radians * (180.0F / pi); }

} // namespace caors::math

#endif
