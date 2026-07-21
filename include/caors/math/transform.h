// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_MATH_TRANSFORM_H_
#define CAORS_MATH_TRANSFORM_H_

#include "caors/math/matrix.h"
#include "caors/math/quaternion.h"

#include <cmath>

namespace caors::math {

/** @brief 创建平移矩阵。矩阵按列向量语义使用，即 `matrix * vector`。 */
template <typename T> [[nodiscard]] constexpr mat4_t<T> translation(vec3_t<T> offset) noexcept {
  mat4_t<T> result = mat4_t<T>::identity();
  result(0, 3) = offset.x;
  result(1, 3) = offset.y;
  result(2, 3) = offset.z;
  return result;
}

/** @brief 创建缩放矩阵。 */
template <typename T> [[nodiscard]] constexpr mat4_t<T> scaling(vec3_t<T> scale) noexcept {
  mat4_t<T> result = mat4_t<T>::identity();
  result(0, 0) = scale.x;
  result(1, 1) = scale.y;
  result(2, 2) = scale.z;
  return result;
}

/** @brief 创建绕 X 轴旋转的矩阵，角度单位为弧度。 */
template <typename T> [[nodiscard]] inline mat4_t<T> rotation_x(T radians) noexcept {
  const T cosine = static_cast<T>(std::cos(radians));
  const T sine = static_cast<T>(std::sin(radians));
  mat4_t<T> result = mat4_t<T>::identity();
  result(1, 1) = cosine;
  result(1, 2) = -sine;
  result(2, 1) = sine;
  result(2, 2) = cosine;
  return result;
}

/** @brief 创建绕 Y 轴旋转的矩阵，角度单位为弧度。 */
template <typename T> [[nodiscard]] inline mat4_t<T> rotation_y(T radians) noexcept {
  const T cosine = static_cast<T>(std::cos(radians));
  const T sine = static_cast<T>(std::sin(radians));
  mat4_t<T> result = mat4_t<T>::identity();
  result(0, 0) = cosine;
  result(0, 2) = sine;
  result(2, 0) = -sine;
  result(2, 2) = cosine;
  return result;
}

/** @brief 创建绕 Z 轴旋转的矩阵，角度单位为弧度。 */
template <typename T> [[nodiscard]] inline mat4_t<T> rotation_z(T radians) noexcept {
  const T cosine = static_cast<T>(std::cos(radians));
  const T sine = static_cast<T>(std::sin(radians));
  mat4_t<T> result = mat4_t<T>::identity();
  result(0, 0) = cosine;
  result(0, 1) = -sine;
  result(1, 0) = sine;
  result(1, 1) = cosine;
  return result;
}

/** @brief 将四元数转换为旋转矩阵。 */
template <typename T> [[nodiscard]] inline mat4_t<T> rotation(quat_t<T> value) noexcept {
  const quat_t<T> unit = normalized(value);
  const T xx = unit.x * unit.x;
  const T yy = unit.y * unit.y;
  const T zz = unit.z * unit.z;
  const T xy = unit.x * unit.y;
  const T xz = unit.x * unit.z;
  const T yz = unit.y * unit.z;
  const T xw = unit.x * unit.w;
  const T yw = unit.y * unit.w;
  const T zw = unit.z * unit.w;

  mat4_t<T> result = mat4_t<T>::identity();
  result(0, 0) = T{1} - T{2} * (yy + zz);
  result(0, 1) = T{2} * (xy - zw);
  result(0, 2) = T{2} * (xz + yw);
  result(1, 0) = T{2} * (xy + zw);
  result(1, 1) = T{1} - T{2} * (xx + zz);
  result(1, 2) = T{2} * (yz - xw);
  result(2, 0) = T{2} * (xz - yw);
  result(2, 1) = T{2} * (yz + xw);
  result(2, 2) = T{1} - T{2} * (xx + yy);
  return result;
}

/** @brief 创建平移、旋转、缩放组合矩阵。 */
template <typename T>
[[nodiscard]] inline mat4_t<T> trs(vec3_t<T> position, quat_t<T> orientation,
                                   vec3_t<T> scale) noexcept {
  return translation(position) * rotation(orientation) * scaling(scale);
}

/** @brief 按点语义变换三维坐标，使用 `w = 1`，不做透视除法。 */
template <typename T>
[[nodiscard]] constexpr vec3_t<T> transform_point(const mat4_t<T>& matrix,
                                                  vec3_t<T> point) noexcept {
  const vec4_t<T> transformed = matrix * vec4_t<T>{point.x, point.y, point.z, T{1}};
  return {transformed.x, transformed.y, transformed.z};
}

/** @brief 按向量语义变换三维值，使用 `w = 0`。 */
template <typename T>
[[nodiscard]] constexpr vec3_t<T> transform_vector(const mat4_t<T>& matrix,
                                                   vec3_t<T> vector) noexcept {
  const vec4_t<T> transformed = matrix * vec4_t<T>{vector.x, vector.y, vector.z, T{}};
  return {transformed.x, transformed.y, transformed.z};
}

/** @brief 按方向语义变换三维值，并归一化结果。 */
template <typename T>
[[nodiscard]] inline vec3_t<T> transform_direction(const mat4_t<T>& matrix,
                                                   vec3_t<T> direction) noexcept {
  return normalized(transform_vector(matrix, direction));
}

} // namespace caors::math

#endif
