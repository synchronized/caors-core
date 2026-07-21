// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_MATH_PROJECTION_H_
#define CAORS_MATH_PROJECTION_H_

#include "caors/math/geometry.h"
#include "caors/math/matrix.h"
#include "caors/math/rect.h"

#include <cmath>
#include <type_traits>

namespace caors::math {

/** @brief 视图/投影矩阵的手系约定。 */
enum class handedness {
  right,
  left,
};

/** @brief 裁剪空间深度范围约定。 */
enum class clip_depth {
  zero_to_one,
  negative_one_to_one,
};

namespace detail {

template <typename T>
[[nodiscard]] constexpr vec4_t<T> matrix_row(const mat4_t<T>& matrix, std::size_t row) noexcept {
  return {matrix(row, 0), matrix(row, 1), matrix(row, 2), matrix(row, 3)};
}

template <typename T> [[nodiscard]] inline plane_t<T> plane_from_coefficients(vec4_t<T> value) {
  return normalized(plane_t<T>{{value.x, value.y, value.z}, value.w});
}

} // namespace detail

/** @brief 创建 look-at 视图矩阵。 */
template <handedness Handedness, typename T>
[[nodiscard]] inline mat4_t<T> look_at(vec3_t<T> eye, vec3_t<T> target, vec3_t<T> up) noexcept {
  const vec3_t<T> forward = normalized(target - eye);
  mat4_t<T> result = mat4_t<T>::identity();

  if constexpr (Handedness == handedness::right) {
    const vec3_t<T> side = normalized(cross(forward, up));
    const vec3_t<T> camera_up = cross(side, forward);

    result(0, 0) = side.x;
    result(0, 1) = side.y;
    result(0, 2) = side.z;
    result(0, 3) = -dot(side, eye);
    result(1, 0) = camera_up.x;
    result(1, 1) = camera_up.y;
    result(1, 2) = camera_up.z;
    result(1, 3) = -dot(camera_up, eye);
    result(2, 0) = -forward.x;
    result(2, 1) = -forward.y;
    result(2, 2) = -forward.z;
    result(2, 3) = dot(forward, eye);
  } else {
    const vec3_t<T> side = normalized(cross(up, forward));
    const vec3_t<T> camera_up = cross(forward, side);

    result(0, 0) = side.x;
    result(0, 1) = side.y;
    result(0, 2) = side.z;
    result(0, 3) = -dot(side, eye);
    result(1, 0) = camera_up.x;
    result(1, 1) = camera_up.y;
    result(1, 2) = camera_up.z;
    result(1, 3) = -dot(camera_up, eye);
    result(2, 0) = forward.x;
    result(2, 1) = forward.y;
    result(2, 2) = forward.z;
    result(2, 3) = -dot(forward, eye);
  }

  return result;
}

/** @brief 创建透视投影矩阵，`fov_y` 使用弧度。 */
template <handedness Handedness, clip_depth Depth, typename T>
[[nodiscard]] inline mat4_t<T> perspective(T fov_y, T aspect, T near_z, T far_z) noexcept {
  const T focal_length = T{1} / static_cast<T>(std::tan(fov_y * T{0.5}));
  mat4_t<T> result{};
  result(0, 0) = focal_length / aspect;
  result(1, 1) = focal_length;

  if constexpr (Handedness == handedness::right) {
    if constexpr (Depth == clip_depth::zero_to_one) {
      result(2, 2) = far_z / (near_z - far_z);
      result(2, 3) = (far_z * near_z) / (near_z - far_z);
    } else {
      result(2, 2) = (far_z + near_z) / (near_z - far_z);
      result(2, 3) = (T{2} * far_z * near_z) / (near_z - far_z);
    }
    result(3, 2) = -T{1};
  } else {
    if constexpr (Depth == clip_depth::zero_to_one) {
      result(2, 2) = far_z / (far_z - near_z);
      result(2, 3) = -(far_z * near_z) / (far_z - near_z);
    } else {
      result(2, 2) = (far_z + near_z) / (far_z - near_z);
      result(2, 3) = -(T{2} * far_z * near_z) / (far_z - near_z);
    }
    result(3, 2) = T{1};
  }

  return result;
}

/** @brief 创建正交投影矩阵。 */
template <handedness Handedness, clip_depth Depth, typename T>
[[nodiscard]] constexpr mat4_t<T> orthographic(T left, T right, T bottom, T top, T near_z,
                                               T far_z) noexcept {
  mat4_t<T> result = mat4_t<T>::identity();
  result(0, 0) = T{2} / (right - left);
  result(0, 3) = -(right + left) / (right - left);
  result(1, 1) = T{2} / (top - bottom);
  result(1, 3) = -(top + bottom) / (top - bottom);

  if constexpr (Handedness == handedness::right) {
    if constexpr (Depth == clip_depth::zero_to_one) {
      result(2, 2) = T{1} / (near_z - far_z);
      result(2, 3) = near_z / (near_z - far_z);
    } else {
      result(2, 2) = T{2} / (near_z - far_z);
      result(2, 3) = (far_z + near_z) / (near_z - far_z);
    }
  } else {
    if constexpr (Depth == clip_depth::zero_to_one) {
      result(2, 2) = T{1} / (far_z - near_z);
      result(2, 3) = -near_z / (far_z - near_z);
    } else {
      result(2, 2) = T{2} / (far_z - near_z);
      result(2, 3) = -(far_z + near_z) / (far_z - near_z);
    }
  }

  return result;
}

/** @brief 按列向量语义组合 view-projection 矩阵。 */
template <typename T>
[[nodiscard]] constexpr mat4_t<T> view_projection(const mat4_t<T>& projection,
                                                  const mat4_t<T>& view) noexcept {
  return projection * view;
}

/** @brief 创建透视 view-projection 矩阵。 */
template <handedness Handedness, clip_depth Depth, typename T>
[[nodiscard]] inline mat4_t<T> perspective_view_projection(vec3_t<T> eye, vec3_t<T> target,
                                                           vec3_t<T> up, T fov_y, T aspect,
                                                           T near_z, T far_z) noexcept {
  return view_projection(perspective<Handedness, Depth>(fov_y, aspect, near_z, far_z),
                         look_at<Handedness>(eye, target, up));
}

/** @brief 创建正交 view-projection 矩阵。 */
template <handedness Handedness, clip_depth Depth, typename T>
[[nodiscard]] inline mat4_t<T> orthographic_view_projection(vec3_t<T> eye, vec3_t<T> target,
                                                            vec3_t<T> up, T left, T right, T bottom,
                                                            T top, T near_z, T far_z) noexcept {
  return view_projection(orthographic<Handedness, Depth>(left, right, bottom, top, near_z, far_z),
                         look_at<Handedness>(eye, target, up));
}

/** @brief 从 view-projection 矩阵提取视锥体平面。 */
template <clip_depth Depth, typename T>
[[nodiscard]] inline frustum_t<T> extract_frustum(const mat4_t<T>& view_projection_matrix) {
  const vec4_t<T> row0 = detail::matrix_row(view_projection_matrix, 0);
  const vec4_t<T> row1 = detail::matrix_row(view_projection_matrix, 1);
  const vec4_t<T> row2 = detail::matrix_row(view_projection_matrix, 2);
  const vec4_t<T> row3 = detail::matrix_row(view_projection_matrix, 3);

  frustum_t<T> result{};
  result.left = detail::plane_from_coefficients(row3 + row0);
  result.right = detail::plane_from_coefficients(row3 - row0);
  result.bottom = detail::plane_from_coefficients(row3 + row1);
  result.top = detail::plane_from_coefficients(row3 - row1);
  if constexpr (Depth == clip_depth::zero_to_one) {
    result.near_plane = detail::plane_from_coefficients(row2);
  } else {
    result.near_plane = detail::plane_from_coefficients(row3 + row2);
  }
  result.far_plane = detail::plane_from_coefficients(row3 - row2);
  return result;
}

/** @brief 把世界坐标投影到 viewport 坐标；viewport 的 Y 轴方向由调用方约定。 */
template <clip_depth Depth, typename T>
[[nodiscard]] constexpr vec3_t<T> project_point(const mat4_t<T>& view_projection_matrix,
                                                rect_t<T> viewport, vec3_t<T> point,
                                                T min_depth = T{}, T max_depth = T{1}) noexcept {
  const vec4_t<T> clip = view_projection_matrix * vec4_t<T>{point.x, point.y, point.z, T{1}};
  const vec3_t<T> ndc{clip.x / clip.w, clip.y / clip.w, clip.z / clip.w};
  const T normalized_depth = Depth == clip_depth::zero_to_one ? ndc.z : (ndc.z + T{1}) * T{0.5};

  return {
      viewport.position.x + (ndc.x + T{1}) * viewport.size.x * T{0.5},
      viewport.position.y + (ndc.y + T{1}) * viewport.size.y * T{0.5},
      min_depth + (max_depth - min_depth) * normalized_depth,
  };
}

/** @brief 使用已经求好的 inverse view-projection 把 viewport 坐标反投影为世界坐标。 */
template <clip_depth Depth, typename T>
  requires std::is_floating_point_v<T>
[[nodiscard]] constexpr bool
try_unproject_point(const mat4_t<T>& inverse_view_projection, rect_t<T> viewport, vec3_t<T> point,
                    vec3_t<T>& output, T min_depth = T{}, T max_depth = T{1},
                    T epsilon = static_cast<T>(0.000001)) noexcept {
  if (abs(viewport.size.x) <= epsilon || abs(viewport.size.y) <= epsilon ||
      abs(max_depth - min_depth) <= epsilon) {
    output = vec3_t<T>::zero();
    return false;
  }

  const T ndc_x = ((point.x - viewport.position.x) / viewport.size.x) * T{2} - T{1};
  const T ndc_y = ((point.y - viewport.position.y) / viewport.size.y) * T{2} - T{1};
  const T depth01 = (point.z - min_depth) / (max_depth - min_depth);
  const T ndc_z = Depth == clip_depth::zero_to_one ? depth01 : depth01 * T{2} - T{1};
  const vec4_t<T> world = inverse_view_projection * vec4_t<T>{ndc_x, ndc_y, ndc_z, T{1}};
  if (abs(world.w) <= epsilon) {
    output = vec3_t<T>::zero();
    return false;
  }

  output = {world.x / world.w, world.y / world.w, world.z / world.w};
  return true;
}

/** @brief 求逆 view-projection 后反投影 viewport 坐标。 */
template <clip_depth Depth, typename T>
  requires std::is_floating_point_v<T>
[[nodiscard]] constexpr bool try_unproject_point_from_view_projection(
    const mat4_t<T>& view_projection_matrix, rect_t<T> viewport, vec3_t<T> point, vec3_t<T>& output,
    T min_depth = T{}, T max_depth = T{1}, T epsilon = static_cast<T>(0.000001)) noexcept {
  mat4_t<T> inverse_view_projection = mat4_t<T>::zero();
  if (!try_inverse(view_projection_matrix, inverse_view_projection, epsilon)) {
    output = vec3_t<T>::zero();
    return false;
  }

  return try_unproject_point<Depth>(inverse_view_projection, viewport, point, output, min_depth,
                                    max_depth, epsilon);
}

[[nodiscard]] inline mat4 look_at_rh(vec3 eye, vec3 target, vec3 up) noexcept {
  return look_at<handedness::right>(eye, target, up);
}

[[nodiscard]] inline mat4 look_at_lh(vec3 eye, vec3 target, vec3 up) noexcept {
  return look_at<handedness::left>(eye, target, up);
}

[[nodiscard]] inline mat4 perspective_rh_zo(float fov_y, float aspect, float near_z,
                                            float far_z) noexcept {
  return perspective<handedness::right, clip_depth::zero_to_one>(fov_y, aspect, near_z, far_z);
}

[[nodiscard]] inline mat4 perspective_rh_no(float fov_y, float aspect, float near_z,
                                            float far_z) noexcept {
  return perspective<handedness::right, clip_depth::negative_one_to_one>(fov_y, aspect, near_z,
                                                                         far_z);
}

[[nodiscard]] inline mat4 perspective_lh_zo(float fov_y, float aspect, float near_z,
                                            float far_z) noexcept {
  return perspective<handedness::left, clip_depth::zero_to_one>(fov_y, aspect, near_z, far_z);
}

[[nodiscard]] inline mat4 perspective_lh_no(float fov_y, float aspect, float near_z,
                                            float far_z) noexcept {
  return perspective<handedness::left, clip_depth::negative_one_to_one>(fov_y, aspect, near_z,
                                                                        far_z);
}

[[nodiscard]] constexpr mat4 orthographic_rh_zo(float left, float right, float bottom, float top,
                                                float near_z, float far_z) noexcept {
  return orthographic<handedness::right, clip_depth::zero_to_one>(left, right, bottom, top, near_z,
                                                                  far_z);
}

[[nodiscard]] constexpr mat4 orthographic_rh_no(float left, float right, float bottom, float top,
                                                float near_z, float far_z) noexcept {
  return orthographic<handedness::right, clip_depth::negative_one_to_one>(left, right, bottom, top,
                                                                          near_z, far_z);
}

[[nodiscard]] constexpr mat4 orthographic_lh_zo(float left, float right, float bottom, float top,
                                                float near_z, float far_z) noexcept {
  return orthographic<handedness::left, clip_depth::zero_to_one>(left, right, bottom, top, near_z,
                                                                 far_z);
}

[[nodiscard]] constexpr mat4 orthographic_lh_no(float left, float right, float bottom, float top,
                                                float near_z, float far_z) noexcept {
  return orthographic<handedness::left, clip_depth::negative_one_to_one>(left, right, bottom, top,
                                                                         near_z, far_z);
}

} // namespace caors::math

#endif
