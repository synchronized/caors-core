// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_MATH_MATRIX_H_
#define CAORS_MATH_MATRIX_H_

#include "caors/math/vector.h"

#include <cstddef>

namespace caors::math {

/** @brief 3x3 单精度矩阵，按行主序存储。 */
struct mat3 {
  float values[9]{};

  [[nodiscard]] static constexpr mat3 zero() noexcept { return {}; }

  [[nodiscard]] static constexpr mat3 identity() noexcept {
    mat3 result{};
    result(0, 0) = 1.0F;
    result(1, 1) = 1.0F;
    result(2, 2) = 1.0F;
    return result;
  }

  [[nodiscard]] constexpr float& operator()(std::size_t row, std::size_t column) noexcept {
    return values[row * 3U + column];
  }

  [[nodiscard]] constexpr const float& operator()(std::size_t row,
                                                  std::size_t column) const noexcept {
    return values[row * 3U + column];
  }
};

/** @brief 4x4 单精度矩阵，按行主序存储。 */
struct mat4 {
  float values[16]{};

  [[nodiscard]] static constexpr mat4 zero() noexcept { return {}; }

  [[nodiscard]] static constexpr mat4 identity() noexcept {
    mat4 result{};
    result(0, 0) = 1.0F;
    result(1, 1) = 1.0F;
    result(2, 2) = 1.0F;
    result(3, 3) = 1.0F;
    return result;
  }

  [[nodiscard]] constexpr float& operator()(std::size_t row, std::size_t column) noexcept {
    return values[row * 4U + column];
  }

  [[nodiscard]] constexpr const float& operator()(std::size_t row,
                                                  std::size_t column) const noexcept {
    return values[row * 4U + column];
  }
};

[[nodiscard]] constexpr bool operator==(const mat3& lhs, const mat3& rhs) noexcept {
  for (std::size_t index = 0; index < 9U; ++index) {
    if (lhs.values[index] != rhs.values[index]) {
      return false;
    }
  }
  return true;
}

[[nodiscard]] constexpr bool operator==(const mat4& lhs, const mat4& rhs) noexcept {
  for (std::size_t index = 0; index < 16U; ++index) {
    if (lhs.values[index] != rhs.values[index]) {
      return false;
    }
  }
  return true;
}

[[nodiscard]] constexpr mat3 operator*(const mat3& lhs, const mat3& rhs) noexcept {
  mat3 result{};
  for (std::size_t row = 0; row < 3U; ++row) {
    for (std::size_t column = 0; column < 3U; ++column) {
      for (std::size_t item = 0; item < 3U; ++item) {
        result(row, column) += lhs(row, item) * rhs(item, column);
      }
    }
  }
  return result;
}

[[nodiscard]] constexpr mat4 operator*(const mat4& lhs, const mat4& rhs) noexcept {
  mat4 result{};
  for (std::size_t row = 0; row < 4U; ++row) {
    for (std::size_t column = 0; column < 4U; ++column) {
      for (std::size_t item = 0; item < 4U; ++item) {
        result(row, column) += lhs(row, item) * rhs(item, column);
      }
    }
  }
  return result;
}

[[nodiscard]] constexpr vec3 operator*(const mat3& matrix, vec3 value) noexcept {
  return {
      matrix(0, 0) * value.x + matrix(0, 1) * value.y + matrix(0, 2) * value.z,
      matrix(1, 0) * value.x + matrix(1, 1) * value.y + matrix(1, 2) * value.z,
      matrix(2, 0) * value.x + matrix(2, 1) * value.y + matrix(2, 2) * value.z,
  };
}

[[nodiscard]] constexpr vec4 operator*(const mat4& matrix, vec4 value) noexcept {
  return {
      matrix(0, 0) * value.x + matrix(0, 1) * value.y + matrix(0, 2) * value.z +
          matrix(0, 3) * value.w,
      matrix(1, 0) * value.x + matrix(1, 1) * value.y + matrix(1, 2) * value.z +
          matrix(1, 3) * value.w,
      matrix(2, 0) * value.x + matrix(2, 1) * value.y + matrix(2, 2) * value.z +
          matrix(2, 3) * value.w,
      matrix(3, 0) * value.x + matrix(3, 1) * value.y + matrix(3, 2) * value.z +
          matrix(3, 3) * value.w,
  };
}

[[nodiscard]] constexpr bool nearly_equal(const mat3& lhs, const mat3& rhs,
                                          float epsilon = 0.00001F) noexcept {
  for (std::size_t index = 0; index < 9U; ++index) {
    if (!nearly_equal(lhs.values[index], rhs.values[index], epsilon)) {
      return false;
    }
  }
  return true;
}

[[nodiscard]] constexpr bool nearly_equal(const mat4& lhs, const mat4& rhs,
                                          float epsilon = 0.00001F) noexcept {
  for (std::size_t index = 0; index < 16U; ++index) {
    if (!nearly_equal(lhs.values[index], rhs.values[index], epsilon)) {
      return false;
    }
  }
  return true;
}

} // namespace caors::math

#endif
