// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_MATH_TRANSFORM_H_
#define CAORS_MATH_TRANSFORM_H_

#include "caors/math/matrix.h"
#include "caors/math/quaternion.h"

#include <cmath>

namespace caors::math {

/** @brief 创建平移矩阵。矩阵按列向量语义使用，即 `matrix * vector`。 */
[[nodiscard]] constexpr mat4 translation(vec3 offset) noexcept {
  mat4 result = mat4::identity();
  result(0, 3) = offset.x;
  result(1, 3) = offset.y;
  result(2, 3) = offset.z;
  return result;
}

/** @brief 创建缩放矩阵。 */
[[nodiscard]] constexpr mat4 scaling(vec3 scale) noexcept {
  mat4 result = mat4::identity();
  result(0, 0) = scale.x;
  result(1, 1) = scale.y;
  result(2, 2) = scale.z;
  return result;
}

/** @brief 创建绕 X 轴旋转的矩阵，角度单位为弧度。 */
[[nodiscard]] inline mat4 rotation_x(float radians) noexcept {
  const float cosine = std::cos(radians);
  const float sine = std::sin(radians);
  mat4 result = mat4::identity();
  result(1, 1) = cosine;
  result(1, 2) = -sine;
  result(2, 1) = sine;
  result(2, 2) = cosine;
  return result;
}

/** @brief 创建绕 Y 轴旋转的矩阵，角度单位为弧度。 */
[[nodiscard]] inline mat4 rotation_y(float radians) noexcept {
  const float cosine = std::cos(radians);
  const float sine = std::sin(radians);
  mat4 result = mat4::identity();
  result(0, 0) = cosine;
  result(0, 2) = sine;
  result(2, 0) = -sine;
  result(2, 2) = cosine;
  return result;
}

/** @brief 创建绕 Z 轴旋转的矩阵，角度单位为弧度。 */
[[nodiscard]] inline mat4 rotation_z(float radians) noexcept {
  const float cosine = std::cos(radians);
  const float sine = std::sin(radians);
  mat4 result = mat4::identity();
  result(0, 0) = cosine;
  result(0, 1) = -sine;
  result(1, 0) = sine;
  result(1, 1) = cosine;
  return result;
}

/** @brief 将四元数转换为旋转矩阵。 */
[[nodiscard]] inline mat4 rotation(quat value) noexcept {
  const quat unit = normalized(value);
  const float xx = unit.x * unit.x;
  const float yy = unit.y * unit.y;
  const float zz = unit.z * unit.z;
  const float xy = unit.x * unit.y;
  const float xz = unit.x * unit.z;
  const float yz = unit.y * unit.z;
  const float xw = unit.x * unit.w;
  const float yw = unit.y * unit.w;
  const float zw = unit.z * unit.w;

  mat4 result = mat4::identity();
  result(0, 0) = 1.0F - 2.0F * (yy + zz);
  result(0, 1) = 2.0F * (xy - zw);
  result(0, 2) = 2.0F * (xz + yw);
  result(1, 0) = 2.0F * (xy + zw);
  result(1, 1) = 1.0F - 2.0F * (xx + zz);
  result(1, 2) = 2.0F * (yz - xw);
  result(2, 0) = 2.0F * (xz - yw);
  result(2, 1) = 2.0F * (yz + xw);
  result(2, 2) = 1.0F - 2.0F * (xx + yy);
  return result;
}

/** @brief 创建平移、旋转、缩放组合矩阵。 */
[[nodiscard]] inline mat4 trs(vec3 position, quat orientation, vec3 scale) noexcept {
  return translation(position) * rotation(orientation) * scaling(scale);
}

/** @brief 按点语义变换三维坐标，使用 `w = 1`，不做透视除法。 */
[[nodiscard]] constexpr vec3 transform_point(const mat4& matrix, vec3 point) noexcept {
  const vec4 transformed = matrix * vec4{point.x, point.y, point.z, 1.0F};
  return {transformed.x, transformed.y, transformed.z};
}

/** @brief 按向量语义变换三维值，使用 `w = 0`。 */
[[nodiscard]] constexpr vec3 transform_vector(const mat4& matrix, vec3 vector) noexcept {
  const vec4 transformed = matrix * vec4{vector.x, vector.y, vector.z, 0.0F};
  return {transformed.x, transformed.y, transformed.z};
}

/** @brief 按方向语义变换三维值，并归一化结果。 */
[[nodiscard]] inline vec3 transform_direction(const mat4& matrix, vec3 direction) noexcept {
  return normalized(transform_vector(matrix, direction));
}

} // namespace caors::math

#endif
