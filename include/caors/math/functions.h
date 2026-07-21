// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_MATH_FUNCTIONS_H_
#define CAORS_MATH_FUNCTIONS_H_

#include "caors/math/vector.h"

#include <type_traits>

namespace caors::math {

template <typename T, typename U>
  requires(std::is_arithmetic_v<T> && std::is_arithmetic_v<U>)
[[nodiscard]] constexpr common_scalar_t<T, U> min(T lhs, U rhs) noexcept {
  return lhs < rhs ? lhs : rhs;
}

template <typename T, typename U>
[[nodiscard]] constexpr vec2_t<common_scalar_t<T, U>> min(vec2_t<T> lhs, vec2_t<U> rhs) noexcept {
  return {min(lhs.x, rhs.x), min(lhs.y, rhs.y)};
}

template <typename T, typename U>
[[nodiscard]] constexpr vec3_t<common_scalar_t<T, U>> min(vec3_t<T> lhs, vec3_t<U> rhs) noexcept {
  return {min(lhs.x, rhs.x), min(lhs.y, rhs.y), min(lhs.z, rhs.z)};
}

template <typename T, typename U>
[[nodiscard]] constexpr vec4_t<common_scalar_t<T, U>> min(vec4_t<T> lhs, vec4_t<U> rhs) noexcept {
  return {min(lhs.x, rhs.x), min(lhs.y, rhs.y), min(lhs.z, rhs.z), min(lhs.w, rhs.w)};
}

template <typename T, typename U>
  requires(std::is_arithmetic_v<T> && std::is_arithmetic_v<U>)
[[nodiscard]] constexpr common_scalar_t<T, U> max(T lhs, U rhs) noexcept {
  return lhs > rhs ? lhs : rhs;
}

template <typename T, typename U>
[[nodiscard]] constexpr vec2_t<common_scalar_t<T, U>> max(vec2_t<T> lhs, vec2_t<U> rhs) noexcept {
  return {max(lhs.x, rhs.x), max(lhs.y, rhs.y)};
}

template <typename T, typename U>
[[nodiscard]] constexpr vec3_t<common_scalar_t<T, U>> max(vec3_t<T> lhs, vec3_t<U> rhs) noexcept {
  return {max(lhs.x, rhs.x), max(lhs.y, rhs.y), max(lhs.z, rhs.z)};
}

template <typename T, typename U>
[[nodiscard]] constexpr vec4_t<common_scalar_t<T, U>> max(vec4_t<T> lhs, vec4_t<U> rhs) noexcept {
  return {max(lhs.x, rhs.x), max(lhs.y, rhs.y), max(lhs.z, rhs.z), max(lhs.w, rhs.w)};
}

template <typename T> [[nodiscard]] constexpr vec2_t<T> abs(vec2_t<T> value) noexcept {
  return {abs(value.x), abs(value.y)};
}

template <typename T> [[nodiscard]] constexpr vec3_t<T> abs(vec3_t<T> value) noexcept {
  return {abs(value.x), abs(value.y), abs(value.z)};
}

template <typename T> [[nodiscard]] constexpr vec4_t<T> abs(vec4_t<T> value) noexcept {
  return {abs(value.x), abs(value.y), abs(value.z), abs(value.w)};
}

template <typename T, typename U, typename V>
  requires(std::is_arithmetic_v<T> && std::is_arithmetic_v<U> && std::is_arithmetic_v<V>)
[[nodiscard]] constexpr common_scalar_t<T, U, V> clamp(T value, U minimum, V maximum) noexcept {
  return min(max(value, minimum), maximum);
}

template <typename T, typename U, typename V>
[[nodiscard]] constexpr vec2_t<common_scalar_t<T, U, V>> clamp(vec2_t<T> value, vec2_t<U> minimum,
                                                               vec2_t<V> maximum) noexcept {
  return min(max(value, minimum), maximum);
}

template <typename T, typename U, typename V>
[[nodiscard]] constexpr vec3_t<common_scalar_t<T, U, V>> clamp(vec3_t<T> value, vec3_t<U> minimum,
                                                               vec3_t<V> maximum) noexcept {
  return min(max(value, minimum), maximum);
}

template <typename T, typename U, typename V>
[[nodiscard]] constexpr vec4_t<common_scalar_t<T, U, V>> clamp(vec4_t<T> value, vec4_t<U> minimum,
                                                               vec4_t<V> maximum) noexcept {
  return min(max(value, minimum), maximum);
}

template <typename T, typename U, typename Amount>
  requires(std::is_arithmetic_v<T> && std::is_arithmetic_v<U> && std::is_arithmetic_v<Amount>)
[[nodiscard]] constexpr common_scalar_t<T, U, Amount> lerp(T lhs, U rhs, Amount amount) noexcept {
  return lhs + (rhs - lhs) * amount;
}

template <typename T, typename U, typename Amount>
  requires std::is_arithmetic_v<Amount>
[[nodiscard]] constexpr auto lerp(vec2_t<T> lhs, vec2_t<U> rhs, Amount amount) noexcept {
  return lhs + (rhs - lhs) * amount;
}

template <typename T, typename U, typename Amount>
  requires std::is_arithmetic_v<Amount>
[[nodiscard]] constexpr auto lerp(vec3_t<T> lhs, vec3_t<U> rhs, Amount amount) noexcept {
  return lhs + (rhs - lhs) * amount;
}

template <typename T, typename U, typename Amount>
  requires std::is_arithmetic_v<Amount>
[[nodiscard]] constexpr auto lerp(vec4_t<T> lhs, vec4_t<U> rhs, Amount amount) noexcept {
  return lhs + (rhs - lhs) * amount;
}

template <typename T, typename U, typename V>
  requires(std::is_arithmetic_v<T> && std::is_arithmetic_v<U> && std::is_arithmetic_v<V>)
[[nodiscard]] constexpr common_scalar_t<T, U, V> smoothstep(T edge0, U edge1, V value) noexcept {
  using result_type = common_scalar_t<T, U, V>;
  const result_type amount =
      clamp((value - edge0) / (edge1 - edge0), result_type{}, result_type{1});
  return amount * amount * (result_type{3} - result_type{2} * amount);
}

} // namespace caors::math

#endif
