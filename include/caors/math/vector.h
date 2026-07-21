// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_MATH_VECTOR_H_
#define CAORS_MATH_VECTOR_H_

#include <cmath>

namespace caors::math {

/** @brief 二维单精度向量。 */
struct vec2 {
  float x = 0.0F;
  float y = 0.0F;

  [[nodiscard]] static constexpr vec2 zero() noexcept { return {}; }
  [[nodiscard]] static constexpr vec2 one() noexcept { return {1.0F, 1.0F}; }
};

/** @brief 三维单精度向量。 */
struct vec3 {
  float x = 0.0F;
  float y = 0.0F;
  float z = 0.0F;

  [[nodiscard]] static constexpr vec3 zero() noexcept { return {}; }
  [[nodiscard]] static constexpr vec3 one() noexcept { return {1.0F, 1.0F, 1.0F}; }
};

/** @brief 四维单精度向量。 */
struct vec4 {
  float x = 0.0F;
  float y = 0.0F;
  float z = 0.0F;
  float w = 0.0F;

  [[nodiscard]] static constexpr vec4 zero() noexcept { return {}; }
  [[nodiscard]] static constexpr vec4 one() noexcept { return {1.0F, 1.0F, 1.0F, 1.0F}; }
};

[[nodiscard]] constexpr bool operator==(vec2 lhs, vec2 rhs) noexcept {
  return lhs.x == rhs.x && lhs.y == rhs.y;
}

[[nodiscard]] constexpr bool operator==(vec3 lhs, vec3 rhs) noexcept {
  return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
}

[[nodiscard]] constexpr bool operator==(vec4 lhs, vec4 rhs) noexcept {
  return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z && lhs.w == rhs.w;
}

[[nodiscard]] constexpr vec2 operator-(vec2 value) noexcept { return {-value.x, -value.y}; }

[[nodiscard]] constexpr vec3 operator-(vec3 value) noexcept {
  return {-value.x, -value.y, -value.z};
}

[[nodiscard]] constexpr vec4 operator-(vec4 value) noexcept {
  return {-value.x, -value.y, -value.z, -value.w};
}

[[nodiscard]] constexpr vec2 operator+(vec2 lhs, vec2 rhs) noexcept {
  return {lhs.x + rhs.x, lhs.y + rhs.y};
}

[[nodiscard]] constexpr vec3 operator+(vec3 lhs, vec3 rhs) noexcept {
  return {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
}

[[nodiscard]] constexpr vec4 operator+(vec4 lhs, vec4 rhs) noexcept {
  return {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z, lhs.w + rhs.w};
}

[[nodiscard]] constexpr vec2 operator-(vec2 lhs, vec2 rhs) noexcept {
  return {lhs.x - rhs.x, lhs.y - rhs.y};
}

[[nodiscard]] constexpr vec3 operator-(vec3 lhs, vec3 rhs) noexcept {
  return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
}

[[nodiscard]] constexpr vec4 operator-(vec4 lhs, vec4 rhs) noexcept {
  return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z, lhs.w - rhs.w};
}

[[nodiscard]] constexpr vec2 operator*(vec2 value, float scalar) noexcept {
  return {value.x * scalar, value.y * scalar};
}

[[nodiscard]] constexpr vec3 operator*(vec3 value, float scalar) noexcept {
  return {value.x * scalar, value.y * scalar, value.z * scalar};
}

[[nodiscard]] constexpr vec4 operator*(vec4 value, float scalar) noexcept {
  return {value.x * scalar, value.y * scalar, value.z * scalar, value.w * scalar};
}

[[nodiscard]] constexpr vec2 operator*(float scalar, vec2 value) noexcept { return value * scalar; }

[[nodiscard]] constexpr vec3 operator*(float scalar, vec3 value) noexcept { return value * scalar; }

[[nodiscard]] constexpr vec4 operator*(float scalar, vec4 value) noexcept { return value * scalar; }

[[nodiscard]] constexpr vec2 operator/(vec2 value, float scalar) noexcept {
  return {value.x / scalar, value.y / scalar};
}

[[nodiscard]] constexpr vec3 operator/(vec3 value, float scalar) noexcept {
  return {value.x / scalar, value.y / scalar, value.z / scalar};
}

[[nodiscard]] constexpr vec4 operator/(vec4 value, float scalar) noexcept {
  return {value.x / scalar, value.y / scalar, value.z / scalar, value.w / scalar};
}

constexpr vec2& operator+=(vec2& lhs, vec2 rhs) noexcept {
  lhs = lhs + rhs;
  return lhs;
}

constexpr vec3& operator+=(vec3& lhs, vec3 rhs) noexcept {
  lhs = lhs + rhs;
  return lhs;
}

constexpr vec4& operator+=(vec4& lhs, vec4 rhs) noexcept {
  lhs = lhs + rhs;
  return lhs;
}

constexpr vec2& operator-=(vec2& lhs, vec2 rhs) noexcept {
  lhs = lhs - rhs;
  return lhs;
}

constexpr vec3& operator-=(vec3& lhs, vec3 rhs) noexcept {
  lhs = lhs - rhs;
  return lhs;
}

constexpr vec4& operator-=(vec4& lhs, vec4 rhs) noexcept {
  lhs = lhs - rhs;
  return lhs;
}

constexpr vec2& operator*=(vec2& value, float scalar) noexcept {
  value = value * scalar;
  return value;
}

constexpr vec3& operator*=(vec3& value, float scalar) noexcept {
  value = value * scalar;
  return value;
}

constexpr vec4& operator*=(vec4& value, float scalar) noexcept {
  value = value * scalar;
  return value;
}

constexpr vec2& operator/=(vec2& value, float scalar) noexcept {
  value = value / scalar;
  return value;
}

constexpr vec3& operator/=(vec3& value, float scalar) noexcept {
  value = value / scalar;
  return value;
}

constexpr vec4& operator/=(vec4& value, float scalar) noexcept {
  value = value / scalar;
  return value;
}

[[nodiscard]] constexpr float dot(vec2 lhs, vec2 rhs) noexcept {
  return lhs.x * rhs.x + lhs.y * rhs.y;
}

[[nodiscard]] constexpr float dot(vec3 lhs, vec3 rhs) noexcept {
  return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

[[nodiscard]] constexpr float dot(vec4 lhs, vec4 rhs) noexcept {
  return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z + lhs.w * rhs.w;
}

[[nodiscard]] constexpr vec3 cross(vec3 lhs, vec3 rhs) noexcept {
  return {
      lhs.y * rhs.z - lhs.z * rhs.y,
      lhs.z * rhs.x - lhs.x * rhs.z,
      lhs.x * rhs.y - lhs.y * rhs.x,
  };
}

[[nodiscard]] constexpr float length_squared(vec2 value) noexcept { return dot(value, value); }

[[nodiscard]] constexpr float length_squared(vec3 value) noexcept { return dot(value, value); }

[[nodiscard]] constexpr float length_squared(vec4 value) noexcept { return dot(value, value); }

[[nodiscard]] inline float length(vec2 value) noexcept { return std::sqrt(length_squared(value)); }

[[nodiscard]] inline float length(vec3 value) noexcept { return std::sqrt(length_squared(value)); }

[[nodiscard]] inline float length(vec4 value) noexcept { return std::sqrt(length_squared(value)); }

[[nodiscard]] inline vec2 normalized(vec2 value) noexcept {
  const float value_length = length(value);
  return value_length > 0.0F ? value / value_length : vec2{};
}

[[nodiscard]] inline vec3 normalized(vec3 value) noexcept {
  const float value_length = length(value);
  return value_length > 0.0F ? value / value_length : vec3{};
}

[[nodiscard]] inline vec4 normalized(vec4 value) noexcept {
  const float value_length = length(value);
  return value_length > 0.0F ? value / value_length : vec4{};
}

[[nodiscard]] constexpr float abs(float value) noexcept { return value < 0.0F ? -value : value; }

[[nodiscard]] constexpr bool nearly_equal(float lhs, float rhs, float epsilon = 0.00001F) noexcept {
  return abs(lhs - rhs) <= epsilon;
}

[[nodiscard]] constexpr bool nearly_equal(vec2 lhs, vec2 rhs, float epsilon = 0.00001F) noexcept {
  return nearly_equal(lhs.x, rhs.x, epsilon) && nearly_equal(lhs.y, rhs.y, epsilon);
}

[[nodiscard]] constexpr bool nearly_equal(vec3 lhs, vec3 rhs, float epsilon = 0.00001F) noexcept {
  return nearly_equal(lhs.x, rhs.x, epsilon) && nearly_equal(lhs.y, rhs.y, epsilon) &&
         nearly_equal(lhs.z, rhs.z, epsilon);
}

[[nodiscard]] constexpr bool nearly_equal(vec4 lhs, vec4 rhs, float epsilon = 0.00001F) noexcept {
  return nearly_equal(lhs.x, rhs.x, epsilon) && nearly_equal(lhs.y, rhs.y, epsilon) &&
         nearly_equal(lhs.z, rhs.z, epsilon) && nearly_equal(lhs.w, rhs.w, epsilon);
}

} // namespace caors::math

#endif
