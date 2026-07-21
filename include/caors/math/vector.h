// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_MATH_VECTOR_H_
#define CAORS_MATH_VECTOR_H_

#include <cmath>
#include <cstdint>
#include <type_traits>

namespace caors::math {

template <typename... Types> using common_scalar_t = std::common_type_t<Types...>;

/** @brief 二维向量模板。 */
template <typename T> struct vec2_t {
  using value_type = T;

  T x{};
  T y{};

  [[nodiscard]] static constexpr vec2_t zero() noexcept { return {}; }
  [[nodiscard]] static constexpr vec2_t one() noexcept { return {T{1}, T{1}}; }
};

/** @brief 三维向量模板。 */
template <typename T> struct vec3_t {
  using value_type = T;

  T x{};
  T y{};
  T z{};

  [[nodiscard]] static constexpr vec3_t zero() noexcept { return {}; }
  [[nodiscard]] static constexpr vec3_t one() noexcept { return {T{1}, T{1}, T{1}}; }
};

/** @brief 四维向量模板。 */
template <typename T> struct vec4_t {
  using value_type = T;

  T x{};
  T y{};
  T z{};
  T w{};

  [[nodiscard]] static constexpr vec4_t zero() noexcept { return {}; }
  [[nodiscard]] static constexpr vec4_t one() noexcept { return {T{1}, T{1}, T{1}, T{1}}; }
};

using vec2f = vec2_t<float>;
using vec3f = vec3_t<float>;
using vec4f = vec4_t<float>;
using vec2 = vec2f;
using vec3 = vec3f;
using vec4 = vec4f;

using dvec2 = vec2_t<double>;
using dvec3 = vec3_t<double>;
using dvec4 = vec4_t<double>;
using ivec2 = vec2_t<std::int32_t>;
using ivec3 = vec3_t<std::int32_t>;
using ivec4 = vec4_t<std::int32_t>;
using uvec2 = vec2_t<std::uint32_t>;
using uvec3 = vec3_t<std::uint32_t>;
using uvec4 = vec4_t<std::uint32_t>;

template <typename T, typename U>
[[nodiscard]] constexpr bool operator==(vec2_t<T> lhs, vec2_t<U> rhs) noexcept {
  return lhs.x == rhs.x && lhs.y == rhs.y;
}

template <typename T, typename U>
[[nodiscard]] constexpr bool operator==(vec3_t<T> lhs, vec3_t<U> rhs) noexcept {
  return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
}

template <typename T, typename U>
[[nodiscard]] constexpr bool operator==(vec4_t<T> lhs, vec4_t<U> rhs) noexcept {
  return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z && lhs.w == rhs.w;
}

template <typename T> [[nodiscard]] constexpr vec2_t<T> operator-(vec2_t<T> value) noexcept {
  return {-value.x, -value.y};
}

template <typename T> [[nodiscard]] constexpr vec3_t<T> operator-(vec3_t<T> value) noexcept {
  return {-value.x, -value.y, -value.z};
}

template <typename T> [[nodiscard]] constexpr vec4_t<T> operator-(vec4_t<T> value) noexcept {
  return {-value.x, -value.y, -value.z, -value.w};
}

template <typename T, typename U>
[[nodiscard]] constexpr vec2_t<common_scalar_t<T, U>> operator+(vec2_t<T> lhs,
                                                                vec2_t<U> rhs) noexcept {
  return {lhs.x + rhs.x, lhs.y + rhs.y};
}

template <typename T, typename U>
[[nodiscard]] constexpr vec3_t<common_scalar_t<T, U>> operator+(vec3_t<T> lhs,
                                                                vec3_t<U> rhs) noexcept {
  return {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
}

template <typename T, typename U>
[[nodiscard]] constexpr vec4_t<common_scalar_t<T, U>> operator+(vec4_t<T> lhs,
                                                                vec4_t<U> rhs) noexcept {
  return {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z, lhs.w + rhs.w};
}

template <typename T, typename U>
[[nodiscard]] constexpr vec2_t<common_scalar_t<T, U>> operator-(vec2_t<T> lhs,
                                                                vec2_t<U> rhs) noexcept {
  return {lhs.x - rhs.x, lhs.y - rhs.y};
}

template <typename T, typename U>
[[nodiscard]] constexpr vec3_t<common_scalar_t<T, U>> operator-(vec3_t<T> lhs,
                                                                vec3_t<U> rhs) noexcept {
  return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
}

template <typename T, typename U>
[[nodiscard]] constexpr vec4_t<common_scalar_t<T, U>> operator-(vec4_t<T> lhs,
                                                                vec4_t<U> rhs) noexcept {
  return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z, lhs.w - rhs.w};
}

template <typename T, typename Scalar>
  requires std::is_arithmetic_v<Scalar>
[[nodiscard]] constexpr vec2_t<common_scalar_t<T, Scalar>> operator*(vec2_t<T> value,
                                                                     Scalar scalar) noexcept {
  return {value.x * scalar, value.y * scalar};
}

template <typename T, typename Scalar>
  requires std::is_arithmetic_v<Scalar>
[[nodiscard]] constexpr vec3_t<common_scalar_t<T, Scalar>> operator*(vec3_t<T> value,
                                                                     Scalar scalar) noexcept {
  return {value.x * scalar, value.y * scalar, value.z * scalar};
}

template <typename T, typename Scalar>
  requires std::is_arithmetic_v<Scalar>
[[nodiscard]] constexpr vec4_t<common_scalar_t<T, Scalar>> operator*(vec4_t<T> value,
                                                                     Scalar scalar) noexcept {
  return {value.x * scalar, value.y * scalar, value.z * scalar, value.w * scalar};
}

template <typename Scalar, typename T>
  requires std::is_arithmetic_v<Scalar>
[[nodiscard]] constexpr vec2_t<common_scalar_t<T, Scalar>> operator*(Scalar scalar,
                                                                     vec2_t<T> value) noexcept {
  return value * scalar;
}

template <typename Scalar, typename T>
  requires std::is_arithmetic_v<Scalar>
[[nodiscard]] constexpr vec3_t<common_scalar_t<T, Scalar>> operator*(Scalar scalar,
                                                                     vec3_t<T> value) noexcept {
  return value * scalar;
}

template <typename Scalar, typename T>
  requires std::is_arithmetic_v<Scalar>
[[nodiscard]] constexpr vec4_t<common_scalar_t<T, Scalar>> operator*(Scalar scalar,
                                                                     vec4_t<T> value) noexcept {
  return value * scalar;
}

template <typename T, typename Scalar>
  requires std::is_arithmetic_v<Scalar>
[[nodiscard]] constexpr vec2_t<common_scalar_t<T, Scalar>> operator/(vec2_t<T> value,
                                                                     Scalar scalar) noexcept {
  return {value.x / scalar, value.y / scalar};
}

template <typename T, typename Scalar>
  requires std::is_arithmetic_v<Scalar>
[[nodiscard]] constexpr vec3_t<common_scalar_t<T, Scalar>> operator/(vec3_t<T> value,
                                                                     Scalar scalar) noexcept {
  return {value.x / scalar, value.y / scalar, value.z / scalar};
}

template <typename T, typename Scalar>
  requires std::is_arithmetic_v<Scalar>
[[nodiscard]] constexpr vec4_t<common_scalar_t<T, Scalar>> operator/(vec4_t<T> value,
                                                                     Scalar scalar) noexcept {
  return {value.x / scalar, value.y / scalar, value.z / scalar, value.w / scalar};
}

template <typename T, typename U>
constexpr vec2_t<T>& operator+=(vec2_t<T>& lhs, vec2_t<U> rhs) noexcept {
  lhs.x = static_cast<T>(lhs.x + rhs.x);
  lhs.y = static_cast<T>(lhs.y + rhs.y);
  return lhs;
}

template <typename T, typename U>
constexpr vec3_t<T>& operator+=(vec3_t<T>& lhs, vec3_t<U> rhs) noexcept {
  lhs.x = static_cast<T>(lhs.x + rhs.x);
  lhs.y = static_cast<T>(lhs.y + rhs.y);
  lhs.z = static_cast<T>(lhs.z + rhs.z);
  return lhs;
}

template <typename T, typename U>
constexpr vec4_t<T>& operator+=(vec4_t<T>& lhs, vec4_t<U> rhs) noexcept {
  lhs.x = static_cast<T>(lhs.x + rhs.x);
  lhs.y = static_cast<T>(lhs.y + rhs.y);
  lhs.z = static_cast<T>(lhs.z + rhs.z);
  lhs.w = static_cast<T>(lhs.w + rhs.w);
  return lhs;
}

template <typename T, typename U>
constexpr vec2_t<T>& operator-=(vec2_t<T>& lhs, vec2_t<U> rhs) noexcept {
  lhs.x = static_cast<T>(lhs.x - rhs.x);
  lhs.y = static_cast<T>(lhs.y - rhs.y);
  return lhs;
}

template <typename T, typename U>
constexpr vec3_t<T>& operator-=(vec3_t<T>& lhs, vec3_t<U> rhs) noexcept {
  lhs.x = static_cast<T>(lhs.x - rhs.x);
  lhs.y = static_cast<T>(lhs.y - rhs.y);
  lhs.z = static_cast<T>(lhs.z - rhs.z);
  return lhs;
}

template <typename T, typename U>
constexpr vec4_t<T>& operator-=(vec4_t<T>& lhs, vec4_t<U> rhs) noexcept {
  lhs.x = static_cast<T>(lhs.x - rhs.x);
  lhs.y = static_cast<T>(lhs.y - rhs.y);
  lhs.z = static_cast<T>(lhs.z - rhs.z);
  lhs.w = static_cast<T>(lhs.w - rhs.w);
  return lhs;
}

template <typename T, typename Scalar>
  requires std::is_arithmetic_v<Scalar>
constexpr vec2_t<T>& operator*=(vec2_t<T>& value, Scalar scalar) noexcept {
  value.x = static_cast<T>(value.x * scalar);
  value.y = static_cast<T>(value.y * scalar);
  return value;
}

template <typename T, typename Scalar>
  requires std::is_arithmetic_v<Scalar>
constexpr vec3_t<T>& operator*=(vec3_t<T>& value, Scalar scalar) noexcept {
  value.x = static_cast<T>(value.x * scalar);
  value.y = static_cast<T>(value.y * scalar);
  value.z = static_cast<T>(value.z * scalar);
  return value;
}

template <typename T, typename Scalar>
  requires std::is_arithmetic_v<Scalar>
constexpr vec4_t<T>& operator*=(vec4_t<T>& value, Scalar scalar) noexcept {
  value.x = static_cast<T>(value.x * scalar);
  value.y = static_cast<T>(value.y * scalar);
  value.z = static_cast<T>(value.z * scalar);
  value.w = static_cast<T>(value.w * scalar);
  return value;
}

template <typename T, typename Scalar>
  requires std::is_arithmetic_v<Scalar>
constexpr vec2_t<T>& operator/=(vec2_t<T>& value, Scalar scalar) noexcept {
  value.x = static_cast<T>(value.x / scalar);
  value.y = static_cast<T>(value.y / scalar);
  return value;
}

template <typename T, typename Scalar>
  requires std::is_arithmetic_v<Scalar>
constexpr vec3_t<T>& operator/=(vec3_t<T>& value, Scalar scalar) noexcept {
  value.x = static_cast<T>(value.x / scalar);
  value.y = static_cast<T>(value.y / scalar);
  value.z = static_cast<T>(value.z / scalar);
  return value;
}

template <typename T, typename Scalar>
  requires std::is_arithmetic_v<Scalar>
constexpr vec4_t<T>& operator/=(vec4_t<T>& value, Scalar scalar) noexcept {
  value.x = static_cast<T>(value.x / scalar);
  value.y = static_cast<T>(value.y / scalar);
  value.z = static_cast<T>(value.z / scalar);
  value.w = static_cast<T>(value.w / scalar);
  return value;
}

template <typename T, typename U>
[[nodiscard]] constexpr common_scalar_t<T, U> dot(vec2_t<T> lhs, vec2_t<U> rhs) noexcept {
  return lhs.x * rhs.x + lhs.y * rhs.y;
}

template <typename T, typename U>
[[nodiscard]] constexpr common_scalar_t<T, U> dot(vec3_t<T> lhs, vec3_t<U> rhs) noexcept {
  return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

template <typename T, typename U>
[[nodiscard]] constexpr common_scalar_t<T, U> dot(vec4_t<T> lhs, vec4_t<U> rhs) noexcept {
  return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z + lhs.w * rhs.w;
}

template <typename T, typename U>
[[nodiscard]] constexpr vec3_t<common_scalar_t<T, U>> cross(vec3_t<T> lhs, vec3_t<U> rhs) noexcept {
  return {
      lhs.y * rhs.z - lhs.z * rhs.y,
      lhs.z * rhs.x - lhs.x * rhs.z,
      lhs.x * rhs.y - lhs.y * rhs.x,
  };
}

template <typename T> [[nodiscard]] constexpr auto length_squared(vec2_t<T> value) noexcept {
  return dot(value, value);
}

template <typename T> [[nodiscard]] constexpr auto length_squared(vec3_t<T> value) noexcept {
  return dot(value, value);
}

template <typename T> [[nodiscard]] constexpr auto length_squared(vec4_t<T> value) noexcept {
  return dot(value, value);
}

template <typename T> [[nodiscard]] inline auto length(vec2_t<T> value) noexcept {
  return std::sqrt(length_squared(value));
}

template <typename T> [[nodiscard]] inline auto length(vec3_t<T> value) noexcept {
  return std::sqrt(length_squared(value));
}

template <typename T> [[nodiscard]] inline auto length(vec4_t<T> value) noexcept {
  return std::sqrt(length_squared(value));
}

template <typename T> [[nodiscard]] inline auto normalized(vec2_t<T> value) noexcept {
  const auto value_length = length(value);
  using result_type = common_scalar_t<T, decltype(value_length)>;
  return value_length > result_type{} ? value / value_length : vec2_t<result_type>{};
}

template <typename T> [[nodiscard]] inline auto normalized(vec3_t<T> value) noexcept {
  const auto value_length = length(value);
  using result_type = common_scalar_t<T, decltype(value_length)>;
  return value_length > result_type{} ? value / value_length : vec3_t<result_type>{};
}

template <typename T> [[nodiscard]] inline auto normalized(vec4_t<T> value) noexcept {
  const auto value_length = length(value);
  using result_type = common_scalar_t<T, decltype(value_length)>;
  return value_length > result_type{} ? value / value_length : vec4_t<result_type>{};
}

template <typename T>
  requires std::is_arithmetic_v<T>
[[nodiscard]] constexpr T abs(T value) noexcept {
  return value < T{} ? -value : value;
}

template <typename T, typename U, typename E>
[[nodiscard]] constexpr bool nearly_equal(T lhs, U rhs, E epsilon) noexcept {
  return abs(lhs - rhs) <= epsilon;
}

template <typename T, typename U> [[nodiscard]] constexpr bool nearly_equal(T lhs, U rhs) noexcept {
  using epsilon_type = common_scalar_t<T, U>;
  return nearly_equal(lhs, rhs, static_cast<epsilon_type>(0.00001));
}

template <typename T, typename U, typename E>
[[nodiscard]] constexpr bool nearly_equal(vec2_t<T> lhs, vec2_t<U> rhs, E epsilon) noexcept {
  return nearly_equal(lhs.x, rhs.x, epsilon) && nearly_equal(lhs.y, rhs.y, epsilon);
}

template <typename T, typename U>
[[nodiscard]] constexpr bool nearly_equal(vec2_t<T> lhs, vec2_t<U> rhs) noexcept {
  using epsilon_type = common_scalar_t<T, U>;
  return nearly_equal(lhs, rhs, static_cast<epsilon_type>(0.00001));
}

template <typename T, typename U, typename E>
[[nodiscard]] constexpr bool nearly_equal(vec3_t<T> lhs, vec3_t<U> rhs, E epsilon) noexcept {
  return nearly_equal(lhs.x, rhs.x, epsilon) && nearly_equal(lhs.y, rhs.y, epsilon) &&
         nearly_equal(lhs.z, rhs.z, epsilon);
}

template <typename T, typename U>
[[nodiscard]] constexpr bool nearly_equal(vec3_t<T> lhs, vec3_t<U> rhs) noexcept {
  using epsilon_type = common_scalar_t<T, U>;
  return nearly_equal(lhs, rhs, static_cast<epsilon_type>(0.00001));
}

template <typename T, typename U, typename E>
[[nodiscard]] constexpr bool nearly_equal(vec4_t<T> lhs, vec4_t<U> rhs, E epsilon) noexcept {
  return nearly_equal(lhs.x, rhs.x, epsilon) && nearly_equal(lhs.y, rhs.y, epsilon) &&
         nearly_equal(lhs.z, rhs.z, epsilon) && nearly_equal(lhs.w, rhs.w, epsilon);
}

template <typename T, typename U>
[[nodiscard]] constexpr bool nearly_equal(vec4_t<T> lhs, vec4_t<U> rhs) noexcept {
  using epsilon_type = common_scalar_t<T, U>;
  return nearly_equal(lhs, rhs, static_cast<epsilon_type>(0.00001));
}

} // namespace caors::math

#endif
