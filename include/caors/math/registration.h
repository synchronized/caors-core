// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_MATH_REGISTRATION_H_
#define CAORS_MATH_REGISTRATION_H_

#include "caors/math.h"
#include "caors/registration.h"

#include <string_view>

namespace caors::math {

inline constexpr std::string_view vec2_meta_name = "caors.math.vec2";
inline constexpr std::string_view vec3_meta_name = "caors.math.vec3";
inline constexpr std::string_view vec4_meta_name = "caors.math.vec4";
inline constexpr std::string_view quat_meta_name = "caors.math.quat";
inline constexpr std::string_view mat3_meta_name = "caors.math.mat3";
inline constexpr std::string_view mat4_meta_name = "caors.math.mat4";
inline constexpr std::string_view rect_meta_name = "caors.math.rect";
inline constexpr std::string_view aabb_meta_name = "caors.math.aabb";

/** @brief 注册 `math::vec2` 的 meta 类型和分量属性。 */
[[nodiscard]] inline error register_vec2(std::string_view name = vec2_meta_name) {
  if (meta::find<vec2>()) {
    return {};
  }

  return registration::type_<vec2>(name)
      .meta()
      .property("x", &vec2::x)
      .property("y", &vec2::y)
      .commit();
}

/** @brief 注册 `math::vec3` 的 meta 类型和分量属性。 */
[[nodiscard]] inline error register_vec3(std::string_view name = vec3_meta_name) {
  if (meta::find<vec3>()) {
    return {};
  }

  return registration::type_<vec3>(name)
      .meta()
      .property("x", &vec3::x)
      .property("y", &vec3::y)
      .property("z", &vec3::z)
      .commit();
}

/** @brief 注册 `math::vec4` 的 meta 类型和分量属性。 */
[[nodiscard]] inline error register_vec4(std::string_view name = vec4_meta_name) {
  if (meta::find<vec4>()) {
    return {};
  }

  return registration::type_<vec4>(name)
      .meta()
      .property("x", &vec4::x)
      .property("y", &vec4::y)
      .property("z", &vec4::z)
      .property("w", &vec4::w)
      .commit();
}

/** @brief 注册 `math::quat` 的 meta 类型和 `(x, y, z, w)` 分量属性。 */
[[nodiscard]] inline error register_quat(std::string_view name = quat_meta_name) {
  if (meta::find<quat>()) {
    return {};
  }

  return registration::type_<quat>(name)
      .meta()
      .property("x", &quat::x)
      .property("y", &quat::y)
      .property("z", &quat::z)
      .property("w", &quat::w)
      .commit();
}

/** @brief 注册 `math::mat3` 的 meta 类型；矩阵元素通过 serialization codec 读写。 */
[[nodiscard]] inline error register_mat3(std::string_view name = mat3_meta_name) {
  if (meta::find<mat3>()) {
    return {};
  }

  return registration::type_<mat3>(name).commit();
}

/** @brief 注册 `math::mat4` 的 meta 类型；矩阵元素通过 serialization codec 读写。 */
[[nodiscard]] inline error register_mat4(std::string_view name = mat4_meta_name) {
  if (meta::find<mat4>()) {
    return {};
  }

  return registration::type_<mat4>(name).commit();
}

/** @brief 注册 `math::rect` 的 meta 类型和 `position` / `size` 属性。 */
[[nodiscard]] inline error register_rect(std::string_view name = rect_meta_name) {
  if (meta::find<rect>()) {
    return {};
  }

  return registration::type_<rect>(name)
      .meta()
      .property("position", &rect::position)
      .property("size", &rect::size)
      .commit();
}

/** @brief 注册 `math::aabb` 的 meta 类型和 `min` / `max` 属性。 */
[[nodiscard]] inline error register_aabb(std::string_view name = aabb_meta_name) {
  if (meta::find<aabb>()) {
    return {};
  }

  return registration::type_<aabb>(name)
      .meta()
      .property("min", &aabb::min)
      .property("max", &aabb::max)
      .commit();
}

/** @brief 注册当前内建数学值类型的 meta 类型。 */
[[nodiscard]] inline error register_meta_types() {
  if (const error status = register_vec2(); status) {
    return status;
  }
  if (const error status = register_vec3(); status) {
    return status;
  }
  if (const error status = register_vec4(); status) {
    return status;
  }
  if (const error status = register_quat(); status) {
    return status;
  }
  if (const error status = register_mat3(); status) {
    return status;
  }
  if (const error status = register_mat4(); status) {
    return status;
  }
  if (const error status = register_rect(); status) {
    return status;
  }
  return register_aabb();
}

} // namespace caors::math

#endif
