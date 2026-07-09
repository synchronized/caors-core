// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_TRAITS_OBJECT_PTR_H_
#define CAORS_TRAITS_OBJECT_PTR_H_

namespace caors::objects {
class object;
} // namespace caors::objects

namespace caors::traits {

/** @brief 对象指针类型语义 traits。 */
template <typename T> struct object_ptr_traits {
  static constexpr bool is_object_ptr = false;
  using object_type = void;
};

/** @brief 对象指针类型的运行期对象访问适配。 */
template <typename T> struct object_ptr_access {
  [[nodiscard]] static objects::object* get(const T&) noexcept { return nullptr; }
  static bool set(T&, objects::object*) noexcept { return false; }
};

} // namespace caors::traits

#endif
