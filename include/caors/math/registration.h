// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_MATH_REGISTRATION_H_
#define CAORS_MATH_REGISTRATION_H_

#include "caors/math.h"
#include "caors/registration.h"

#include <cstdint>
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
inline constexpr std::string_view ray_meta_name = "caors.math.ray";
inline constexpr std::string_view sphere_meta_name = "caors.math.sphere";
inline constexpr std::string_view plane_meta_name = "caors.math.plane";
inline constexpr std::string_view frustum_meta_name = "caors.math.frustum";

template <typename T> inline constexpr std::string_view vec2_meta_name_of = "";
template <typename T> inline constexpr std::string_view vec3_meta_name_of = "";
template <typename T> inline constexpr std::string_view vec4_meta_name_of = "";
template <typename T> inline constexpr std::string_view quat_meta_name_of = "";
template <typename T> inline constexpr std::string_view mat3_meta_name_of = "";
template <typename T> inline constexpr std::string_view mat4_meta_name_of = "";
template <typename T> inline constexpr std::string_view rect_meta_name_of = "";
template <typename T> inline constexpr std::string_view aabb_meta_name_of = "";
template <typename T> inline constexpr std::string_view ray_meta_name_of = "";
template <typename T> inline constexpr std::string_view sphere_meta_name_of = "";
template <typename T> inline constexpr std::string_view plane_meta_name_of = "";
template <typename T> inline constexpr std::string_view frustum_meta_name_of = "";

template <> inline constexpr std::string_view vec2_meta_name_of<float> = vec2_meta_name;
template <> inline constexpr std::string_view vec3_meta_name_of<float> = vec3_meta_name;
template <> inline constexpr std::string_view vec4_meta_name_of<float> = vec4_meta_name;
template <> inline constexpr std::string_view quat_meta_name_of<float> = quat_meta_name;
template <> inline constexpr std::string_view mat3_meta_name_of<float> = mat3_meta_name;
template <> inline constexpr std::string_view mat4_meta_name_of<float> = mat4_meta_name;
template <> inline constexpr std::string_view rect_meta_name_of<float> = rect_meta_name;
template <> inline constexpr std::string_view aabb_meta_name_of<float> = aabb_meta_name;
template <> inline constexpr std::string_view ray_meta_name_of<float> = ray_meta_name;
template <> inline constexpr std::string_view sphere_meta_name_of<float> = sphere_meta_name;
template <> inline constexpr std::string_view plane_meta_name_of<float> = plane_meta_name;
template <> inline constexpr std::string_view frustum_meta_name_of<float> = frustum_meta_name;

template <> inline constexpr std::string_view vec2_meta_name_of<double> = "caors.math.dvec2";
template <> inline constexpr std::string_view vec3_meta_name_of<double> = "caors.math.dvec3";
template <> inline constexpr std::string_view vec4_meta_name_of<double> = "caors.math.dvec4";
template <> inline constexpr std::string_view quat_meta_name_of<double> = "caors.math.dquat";
template <> inline constexpr std::string_view mat3_meta_name_of<double> = "caors.math.dmat3";
template <> inline constexpr std::string_view mat4_meta_name_of<double> = "caors.math.dmat4";
template <> inline constexpr std::string_view rect_meta_name_of<double> = "caors.math.drect";
template <> inline constexpr std::string_view aabb_meta_name_of<double> = "caors.math.daabb";
template <> inline constexpr std::string_view ray_meta_name_of<double> = "caors.math.dray";
template <> inline constexpr std::string_view sphere_meta_name_of<double> = "caors.math.dsphere";
template <> inline constexpr std::string_view plane_meta_name_of<double> = "caors.math.dplane";
template <> inline constexpr std::string_view frustum_meta_name_of<double> = "caors.math.dfrustum";

template <> inline constexpr std::string_view vec2_meta_name_of<std::int32_t> = "caors.math.ivec2";
template <> inline constexpr std::string_view vec3_meta_name_of<std::int32_t> = "caors.math.ivec3";
template <> inline constexpr std::string_view vec4_meta_name_of<std::int32_t> = "caors.math.ivec4";
template <> inline constexpr std::string_view rect_meta_name_of<std::int32_t> = "caors.math.irect";
template <> inline constexpr std::string_view aabb_meta_name_of<std::int32_t> = "caors.math.iaabb";

template <> inline constexpr std::string_view vec2_meta_name_of<std::uint32_t> = "caors.math.uvec2";
template <> inline constexpr std::string_view vec3_meta_name_of<std::uint32_t> = "caors.math.uvec3";
template <> inline constexpr std::string_view vec4_meta_name_of<std::uint32_t> = "caors.math.uvec4";

/** @brief 注册 `math::vec2` 的 meta 类型和分量属性。 */
template <typename T = float>
[[nodiscard]] inline error register_vec2(std::string_view name = vec2_meta_name_of<T>) {
  if (meta::find<vec2_t<T>>()) {
    return {};
  }

  return registration::type_<vec2_t<T>>(name)
      .meta()
      .property("x", &vec2_t<T>::x)
      .property("y", &vec2_t<T>::y)
      .commit();
}

/** @brief 注册 `math::vec3` 的 meta 类型和分量属性。 */
template <typename T = float>
[[nodiscard]] inline error register_vec3(std::string_view name = vec3_meta_name_of<T>) {
  if (meta::find<vec3_t<T>>()) {
    return {};
  }

  return registration::type_<vec3_t<T>>(name)
      .meta()
      .property("x", &vec3_t<T>::x)
      .property("y", &vec3_t<T>::y)
      .property("z", &vec3_t<T>::z)
      .commit();
}

/** @brief 注册 `math::vec4` 的 meta 类型和分量属性。 */
template <typename T = float>
[[nodiscard]] inline error register_vec4(std::string_view name = vec4_meta_name_of<T>) {
  if (meta::find<vec4_t<T>>()) {
    return {};
  }

  return registration::type_<vec4_t<T>>(name)
      .meta()
      .property("x", &vec4_t<T>::x)
      .property("y", &vec4_t<T>::y)
      .property("z", &vec4_t<T>::z)
      .property("w", &vec4_t<T>::w)
      .commit();
}

/** @brief 注册 `math::quat` 的 meta 类型和 `(x, y, z, w)` 分量属性。 */
template <typename T = float>
[[nodiscard]] inline error register_quat(std::string_view name = quat_meta_name_of<T>) {
  if (meta::find<quat_t<T>>()) {
    return {};
  }

  return registration::type_<quat_t<T>>(name)
      .meta()
      .property("x", &quat_t<T>::x)
      .property("y", &quat_t<T>::y)
      .property("z", &quat_t<T>::z)
      .property("w", &quat_t<T>::w)
      .commit();
}

/** @brief 注册 `math::mat3` 的 meta 类型；矩阵元素通过 serialization codec 读写。 */
template <typename T = float>
[[nodiscard]] inline error register_mat3(std::string_view name = mat3_meta_name_of<T>) {
  if (meta::find<mat3_t<T>>()) {
    return {};
  }

  return registration::type_<mat3_t<T>>(name).commit();
}

/** @brief 注册 `math::mat4` 的 meta 类型；矩阵元素通过 serialization codec 读写。 */
template <typename T = float>
[[nodiscard]] inline error register_mat4(std::string_view name = mat4_meta_name_of<T>) {
  if (meta::find<mat4_t<T>>()) {
    return {};
  }

  return registration::type_<mat4_t<T>>(name).commit();
}

/** @brief 注册 `math::rect` 的 meta 类型和 `position` / `size` 属性。 */
template <typename T = float>
[[nodiscard]] inline error register_rect(std::string_view name = rect_meta_name_of<T>) {
  if (meta::find<rect_t<T>>()) {
    return {};
  }

  return registration::type_<rect_t<T>>(name)
      .meta()
      .property("position", &rect_t<T>::position)
      .property("size", &rect_t<T>::size)
      .commit();
}

/** @brief 注册 `math::aabb` 的 meta 类型和 `min` / `max` 属性。 */
template <typename T = float>
[[nodiscard]] inline error register_aabb(std::string_view name = aabb_meta_name_of<T>) {
  if (meta::find<aabb_t<T>>()) {
    return {};
  }

  return registration::type_<aabb_t<T>>(name)
      .meta()
      .property("min", &aabb_t<T>::min)
      .property("max", &aabb_t<T>::max)
      .commit();
}

/** @brief 注册 `math::ray` 的 meta 类型和 `origin` / `direction` 属性。 */
template <typename T = float>
[[nodiscard]] inline error register_ray(std::string_view name = ray_meta_name_of<T>) {
  if (meta::find<ray_t<T>>()) {
    return {};
  }

  return registration::type_<ray_t<T>>(name)
      .meta()
      .property("origin", &ray_t<T>::origin)
      .property("direction", &ray_t<T>::direction)
      .commit();
}

/** @brief 注册 `math::sphere` 的 meta 类型和 `center` / `radius` 属性。 */
template <typename T = float>
[[nodiscard]] inline error register_sphere(std::string_view name = sphere_meta_name_of<T>) {
  if (meta::find<sphere_t<T>>()) {
    return {};
  }

  return registration::type_<sphere_t<T>>(name)
      .meta()
      .property("center", &sphere_t<T>::center)
      .property("radius", &sphere_t<T>::radius)
      .commit();
}

/** @brief 注册 `math::plane` 的 meta 类型和 `normal` / `distance` 属性。 */
template <typename T = float>
[[nodiscard]] inline error register_plane(std::string_view name = plane_meta_name_of<T>) {
  if (meta::find<plane_t<T>>()) {
    return {};
  }

  return registration::type_<plane_t<T>>(name)
      .meta()
      .property("normal", &plane_t<T>::normal)
      .property("distance", &plane_t<T>::distance)
      .commit();
}

/** @brief 注册 `math::frustum` 的 meta 类型和六个裁剪平面属性。 */
template <typename T = float>
[[nodiscard]] inline error register_frustum(std::string_view name = frustum_meta_name_of<T>) {
  if (meta::find<frustum_t<T>>()) {
    return {};
  }

  return registration::type_<frustum_t<T>>(name)
      .meta()
      .property("left", &frustum_t<T>::left)
      .property("right", &frustum_t<T>::right)
      .property("bottom", &frustum_t<T>::bottom)
      .property("top", &frustum_t<T>::top)
      .property("near", &frustum_t<T>::near_plane)
      .property("far", &frustum_t<T>::far_plane)
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
  if (const error status = register_aabb(); status) {
    return status;
  }
  if (const error status = register_ray(); status) {
    return status;
  }
  if (const error status = register_sphere(); status) {
    return status;
  }
  if (const error status = register_plane(); status) {
    return status;
  }
  return register_frustum();
}

} // namespace caors::math

#endif
