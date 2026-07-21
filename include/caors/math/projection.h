// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_MATH_PROJECTION_H_
#define CAORS_MATH_PROJECTION_H_

#include "caors/math/matrix.h"

#include <cmath>

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
