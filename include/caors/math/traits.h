// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_MATH_TRAITS_H_
#define CAORS_MATH_TRAITS_H_

#include "caors/math/aabb.h"
#include "caors/math/geometry.h"
#include "caors/math/matrix.h"
#include "caors/math/quaternion.h"
#include "caors/math/rect.h"
#include "caors/math/vector.h"

#include <cstddef>

namespace caors::math {

template <typename T> struct vector_traits;
template <typename T> struct matrix_traits;
template <typename T> struct quaternion_traits;
template <typename T> struct rect_traits;
template <typename T> struct aabb_traits;
template <typename T> struct ray_traits;
template <typename T> struct sphere_traits;
template <typename T> struct plane_traits;

/** @brief `vec2_t<T>` 的统一向量 traits。 */
template <typename T> struct vector_traits<vec2_t<T>> {
  using value_type = T;
  static constexpr std::size_t size = 2;

  [[nodiscard]] static constexpr value_type& get(vec2_t<T>& value, std::size_t index) noexcept {
    return index == 0U ? value.x : value.y;
  }

  [[nodiscard]] static constexpr const value_type& get(const vec2_t<T>& value,
                                                       std::size_t index) noexcept {
    return index == 0U ? value.x : value.y;
  }
};

/** @brief `vec3_t<T>` 的统一向量 traits。 */
template <typename T> struct vector_traits<vec3_t<T>> {
  using value_type = T;
  static constexpr std::size_t size = 3;

  [[nodiscard]] static constexpr value_type& get(vec3_t<T>& value, std::size_t index) noexcept {
    return index == 0U ? value.x : (index == 1U ? value.y : value.z);
  }

  [[nodiscard]] static constexpr const value_type& get(const vec3_t<T>& value,
                                                       std::size_t index) noexcept {
    return index == 0U ? value.x : (index == 1U ? value.y : value.z);
  }
};

/** @brief `vec4_t<T>` 的统一向量 traits。 */
template <typename T> struct vector_traits<vec4_t<T>> {
  using value_type = T;
  static constexpr std::size_t size = 4;

  [[nodiscard]] static constexpr value_type& get(vec4_t<T>& value, std::size_t index) noexcept {
    return index == 0U ? value.x : (index == 1U ? value.y : (index == 2U ? value.z : value.w));
  }

  [[nodiscard]] static constexpr const value_type& get(const vec4_t<T>& value,
                                                       std::size_t index) noexcept {
    return index == 0U ? value.x : (index == 1U ? value.y : (index == 2U ? value.z : value.w));
  }
};

/** @brief `mat3_t<T>` 的统一矩阵 traits。 */
template <typename T> struct matrix_traits<mat3_t<T>> {
  using value_type = T;
  static constexpr std::size_t rows = 3;
  static constexpr std::size_t columns = 3;

  [[nodiscard]] static constexpr value_type& get(mat3_t<T>& value, std::size_t row,
                                                 std::size_t column) noexcept {
    return value(row, column);
  }

  [[nodiscard]] static constexpr const value_type& get(const mat3_t<T>& value, std::size_t row,
                                                       std::size_t column) noexcept {
    return value(row, column);
  }
};

/** @brief `mat4_t<T>` 的统一矩阵 traits。 */
template <typename T> struct matrix_traits<mat4_t<T>> {
  using value_type = T;
  static constexpr std::size_t rows = 4;
  static constexpr std::size_t columns = 4;

  [[nodiscard]] static constexpr value_type& get(mat4_t<T>& value, std::size_t row,
                                                 std::size_t column) noexcept {
    return value(row, column);
  }

  [[nodiscard]] static constexpr const value_type& get(const mat4_t<T>& value, std::size_t row,
                                                       std::size_t column) noexcept {
    return value(row, column);
  }
};

/** @brief `quat_t<T>` 的统一四元数 traits。 */
template <typename T> struct quaternion_traits<quat_t<T>> {
  using value_type = T;

  [[nodiscard]] static constexpr value_type& x(quat_t<T>& value) noexcept { return value.x; }
  [[nodiscard]] static constexpr const value_type& x(const quat_t<T>& value) noexcept {
    return value.x;
  }

  [[nodiscard]] static constexpr value_type& y(quat_t<T>& value) noexcept { return value.y; }
  [[nodiscard]] static constexpr const value_type& y(const quat_t<T>& value) noexcept {
    return value.y;
  }

  [[nodiscard]] static constexpr value_type& z(quat_t<T>& value) noexcept { return value.z; }
  [[nodiscard]] static constexpr const value_type& z(const quat_t<T>& value) noexcept {
    return value.z;
  }

  [[nodiscard]] static constexpr value_type& w(quat_t<T>& value) noexcept { return value.w; }
  [[nodiscard]] static constexpr const value_type& w(const quat_t<T>& value) noexcept {
    return value.w;
  }
};

/** @brief `rect_t<T>` 的统一矩形 traits。 */
template <typename T> struct rect_traits<rect_t<T>> {
  using value_type = T;
  using vector_type = vec2_t<T>;

  [[nodiscard]] static constexpr vector_type& position(rect_t<T>& value) noexcept {
    return value.position;
  }

  [[nodiscard]] static constexpr const vector_type& position(const rect_t<T>& value) noexcept {
    return value.position;
  }

  [[nodiscard]] static constexpr vector_type& size(rect_t<T>& value) noexcept { return value.size; }
  [[nodiscard]] static constexpr const vector_type& size(const rect_t<T>& value) noexcept {
    return value.size;
  }
};

/** @brief `aabb_t<T>` 的统一 AABB traits。 */
template <typename T> struct aabb_traits<aabb_t<T>> {
  using value_type = T;
  using vector_type = vec3_t<T>;

  [[nodiscard]] static constexpr vector_type& min(aabb_t<T>& value) noexcept { return value.min; }
  [[nodiscard]] static constexpr const vector_type& min(const aabb_t<T>& value) noexcept {
    return value.min;
  }

  [[nodiscard]] static constexpr vector_type& max(aabb_t<T>& value) noexcept { return value.max; }
  [[nodiscard]] static constexpr const vector_type& max(const aabb_t<T>& value) noexcept {
    return value.max;
  }
};

/** @brief `ray_t<T>` 的统一射线 traits。 */
template <typename T> struct ray_traits<ray_t<T>> {
  using value_type = T;
  using vector_type = vec3_t<T>;

  [[nodiscard]] static constexpr vector_type& origin(ray_t<T>& value) noexcept {
    return value.origin;
  }

  [[nodiscard]] static constexpr const vector_type& origin(const ray_t<T>& value) noexcept {
    return value.origin;
  }

  [[nodiscard]] static constexpr vector_type& direction(ray_t<T>& value) noexcept {
    return value.direction;
  }

  [[nodiscard]] static constexpr const vector_type& direction(const ray_t<T>& value) noexcept {
    return value.direction;
  }
};

/** @brief `sphere_t<T>` 的统一球体 traits。 */
template <typename T> struct sphere_traits<sphere_t<T>> {
  using value_type = T;
  using vector_type = vec3_t<T>;

  [[nodiscard]] static constexpr vector_type& center(sphere_t<T>& value) noexcept {
    return value.center;
  }

  [[nodiscard]] static constexpr const vector_type& center(const sphere_t<T>& value) noexcept {
    return value.center;
  }

  [[nodiscard]] static constexpr value_type& radius(sphere_t<T>& value) noexcept {
    return value.radius;
  }

  [[nodiscard]] static constexpr const value_type& radius(const sphere_t<T>& value) noexcept {
    return value.radius;
  }
};

/** @brief `plane_t<T>` 的统一平面 traits。 */
template <typename T> struct plane_traits<plane_t<T>> {
  using value_type = T;
  using vector_type = vec3_t<T>;

  [[nodiscard]] static constexpr vector_type& normal(plane_t<T>& value) noexcept {
    return value.normal;
  }

  [[nodiscard]] static constexpr const vector_type& normal(const plane_t<T>& value) noexcept {
    return value.normal;
  }

  [[nodiscard]] static constexpr value_type& distance(plane_t<T>& value) noexcept {
    return value.distance;
  }

  [[nodiscard]] static constexpr const value_type& distance(const plane_t<T>& value) noexcept {
    return value.distance;
  }
};

} // namespace caors::math

#endif
