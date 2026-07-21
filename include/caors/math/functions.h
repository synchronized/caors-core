// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_MATH_FUNCTIONS_H_
#define CAORS_MATH_FUNCTIONS_H_

#include "caors/math/vector.h"

namespace caors::math {

[[nodiscard]] constexpr float min(float lhs, float rhs) noexcept { return lhs < rhs ? lhs : rhs; }

[[nodiscard]] constexpr vec2 min(vec2 lhs, vec2 rhs) noexcept {
  return {min(lhs.x, rhs.x), min(lhs.y, rhs.y)};
}

[[nodiscard]] constexpr vec3 min(vec3 lhs, vec3 rhs) noexcept {
  return {min(lhs.x, rhs.x), min(lhs.y, rhs.y), min(lhs.z, rhs.z)};
}

[[nodiscard]] constexpr vec4 min(vec4 lhs, vec4 rhs) noexcept {
  return {min(lhs.x, rhs.x), min(lhs.y, rhs.y), min(lhs.z, rhs.z), min(lhs.w, rhs.w)};
}

[[nodiscard]] constexpr float max(float lhs, float rhs) noexcept { return lhs > rhs ? lhs : rhs; }

[[nodiscard]] constexpr vec2 max(vec2 lhs, vec2 rhs) noexcept {
  return {max(lhs.x, rhs.x), max(lhs.y, rhs.y)};
}

[[nodiscard]] constexpr vec3 max(vec3 lhs, vec3 rhs) noexcept {
  return {max(lhs.x, rhs.x), max(lhs.y, rhs.y), max(lhs.z, rhs.z)};
}

[[nodiscard]] constexpr vec4 max(vec4 lhs, vec4 rhs) noexcept {
  return {max(lhs.x, rhs.x), max(lhs.y, rhs.y), max(lhs.z, rhs.z), max(lhs.w, rhs.w)};
}

[[nodiscard]] constexpr vec2 abs(vec2 value) noexcept { return {abs(value.x), abs(value.y)}; }

[[nodiscard]] constexpr vec3 abs(vec3 value) noexcept {
  return {abs(value.x), abs(value.y), abs(value.z)};
}

[[nodiscard]] constexpr vec4 abs(vec4 value) noexcept {
  return {abs(value.x), abs(value.y), abs(value.z), abs(value.w)};
}

[[nodiscard]] constexpr float clamp(float value, float minimum, float maximum) noexcept {
  return min(max(value, minimum), maximum);
}

[[nodiscard]] constexpr vec2 clamp(vec2 value, vec2 minimum, vec2 maximum) noexcept {
  return min(max(value, minimum), maximum);
}

[[nodiscard]] constexpr vec3 clamp(vec3 value, vec3 minimum, vec3 maximum) noexcept {
  return min(max(value, minimum), maximum);
}

[[nodiscard]] constexpr vec4 clamp(vec4 value, vec4 minimum, vec4 maximum) noexcept {
  return min(max(value, minimum), maximum);
}

[[nodiscard]] constexpr float lerp(float lhs, float rhs, float amount) noexcept {
  return lhs + (rhs - lhs) * amount;
}

[[nodiscard]] constexpr vec2 lerp(vec2 lhs, vec2 rhs, float amount) noexcept {
  return lhs + (rhs - lhs) * amount;
}

[[nodiscard]] constexpr vec3 lerp(vec3 lhs, vec3 rhs, float amount) noexcept {
  return lhs + (rhs - lhs) * amount;
}

[[nodiscard]] constexpr vec4 lerp(vec4 lhs, vec4 rhs, float amount) noexcept {
  return lhs + (rhs - lhs) * amount;
}

[[nodiscard]] constexpr float smoothstep(float edge0, float edge1, float value) noexcept {
  const float amount = clamp((value - edge0) / (edge1 - edge0), 0.0F, 1.0F);
  return amount * amount * (3.0F - 2.0F * amount);
}

} // namespace caors::math

#endif
