// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_MATH_MATRIX_H_
#define CAORS_MATH_MATRIX_H_

#include "caors/math/vector.h"

#include <cstddef>

namespace caors::math {

/** @brief 3x3 矩阵模板，按行主序存储。 */
template <typename T> struct mat3_t {
  using value_type = T;

  T values[9]{};

  [[nodiscard]] static constexpr mat3_t zero() noexcept { return {}; }

  [[nodiscard]] static constexpr mat3_t identity() noexcept {
    mat3_t result{};
    result(0, 0) = T{1};
    result(1, 1) = T{1};
    result(2, 2) = T{1};
    return result;
  }

  [[nodiscard]] constexpr T& operator()(std::size_t row, std::size_t column) noexcept {
    return values[row * 3U + column];
  }

  [[nodiscard]] constexpr const T& operator()(std::size_t row, std::size_t column) const noexcept {
    return values[row * 3U + column];
  }
};

/** @brief 4x4 矩阵模板，按行主序存储。 */
template <typename T> struct mat4_t {
  using value_type = T;

  T values[16]{};

  [[nodiscard]] static constexpr mat4_t zero() noexcept { return {}; }

  [[nodiscard]] static constexpr mat4_t identity() noexcept {
    mat4_t result{};
    result(0, 0) = T{1};
    result(1, 1) = T{1};
    result(2, 2) = T{1};
    result(3, 3) = T{1};
    return result;
  }

  [[nodiscard]] constexpr T& operator()(std::size_t row, std::size_t column) noexcept {
    return values[row * 4U + column];
  }

  [[nodiscard]] constexpr const T& operator()(std::size_t row, std::size_t column) const noexcept {
    return values[row * 4U + column];
  }
};

using mat3f = mat3_t<float>;
using mat4f = mat4_t<float>;
using mat3 = mat3f;
using mat4 = mat4f;

using dmat3 = mat3_t<double>;
using dmat4 = mat4_t<double>;

template <typename T, typename U>
[[nodiscard]] constexpr bool operator==(const mat3_t<T>& lhs, const mat3_t<U>& rhs) noexcept {
  for (std::size_t index = 0; index < 9U; ++index) {
    if (lhs.values[index] != rhs.values[index]) {
      return false;
    }
  }
  return true;
}

template <typename T, typename U>
[[nodiscard]] constexpr bool operator==(const mat4_t<T>& lhs, const mat4_t<U>& rhs) noexcept {
  for (std::size_t index = 0; index < 16U; ++index) {
    if (lhs.values[index] != rhs.values[index]) {
      return false;
    }
  }
  return true;
}

template <typename T, typename U>
[[nodiscard]] constexpr mat3_t<common_scalar_t<T, U>> operator*(const mat3_t<T>& lhs,
                                                                const mat3_t<U>& rhs) noexcept {
  mat3_t<common_scalar_t<T, U>> result{};
  for (std::size_t row = 0; row < 3U; ++row) {
    for (std::size_t column = 0; column < 3U; ++column) {
      for (std::size_t item = 0; item < 3U; ++item) {
        result(row, column) += lhs(row, item) * rhs(item, column);
      }
    }
  }
  return result;
}

template <typename T, typename U>
[[nodiscard]] constexpr mat4_t<common_scalar_t<T, U>> operator*(const mat4_t<T>& lhs,
                                                                const mat4_t<U>& rhs) noexcept {
  mat4_t<common_scalar_t<T, U>> result{};
  for (std::size_t row = 0; row < 4U; ++row) {
    for (std::size_t column = 0; column < 4U; ++column) {
      for (std::size_t item = 0; item < 4U; ++item) {
        result(row, column) += lhs(row, item) * rhs(item, column);
      }
    }
  }
  return result;
}

template <typename T, typename U>
[[nodiscard]] constexpr vec3_t<common_scalar_t<T, U>> operator*(const mat3_t<T>& matrix,
                                                                vec3_t<U> value) noexcept {
  return {
      matrix(0, 0) * value.x + matrix(0, 1) * value.y + matrix(0, 2) * value.z,
      matrix(1, 0) * value.x + matrix(1, 1) * value.y + matrix(1, 2) * value.z,
      matrix(2, 0) * value.x + matrix(2, 1) * value.y + matrix(2, 2) * value.z,
  };
}

template <typename T, typename U>
[[nodiscard]] constexpr vec4_t<common_scalar_t<T, U>> operator*(const mat4_t<T>& matrix,
                                                                vec4_t<U> value) noexcept {
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

template <typename T, typename U, typename E>
[[nodiscard]] constexpr bool nearly_equal(const mat3_t<T>& lhs, const mat3_t<U>& rhs,
                                          E epsilon) noexcept {
  for (std::size_t index = 0; index < 9U; ++index) {
    if (!nearly_equal(lhs.values[index], rhs.values[index], epsilon)) {
      return false;
    }
  }
  return true;
}

template <typename T, typename U>
[[nodiscard]] constexpr bool nearly_equal(const mat3_t<T>& lhs, const mat3_t<U>& rhs) noexcept {
  using epsilon_type = common_scalar_t<T, U>;
  return nearly_equal(lhs, rhs, static_cast<epsilon_type>(0.00001));
}

template <typename T, typename U, typename E>
[[nodiscard]] constexpr bool nearly_equal(const mat4_t<T>& lhs, const mat4_t<U>& rhs,
                                          E epsilon) noexcept {
  for (std::size_t index = 0; index < 16U; ++index) {
    if (!nearly_equal(lhs.values[index], rhs.values[index], epsilon)) {
      return false;
    }
  }
  return true;
}

template <typename T, typename U>
[[nodiscard]] constexpr bool nearly_equal(const mat4_t<T>& lhs, const mat4_t<U>& rhs) noexcept {
  using epsilon_type = common_scalar_t<T, U>;
  return nearly_equal(lhs, rhs, static_cast<epsilon_type>(0.00001));
}

} // namespace caors::math

#endif
