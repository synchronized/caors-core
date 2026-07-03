// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_TRAITS_WRAPPER_H_
#define CAORS_TRAITS_WRAPPER_H_

#include "caors/meta/type.h"

namespace caors::traits {

/** @brief C++ wrapper 类型到 meta 类型语义的映射。 */
template <typename T> struct wrapper_traits {
  static constexpr bool is_wrapper = false;
  static constexpr bool nullable = false;
  static constexpr meta::wrapper_semantics semantics = meta::wrapper_semantics::none;
};

/** @brief wrapper 类型的内部值访问适配。 */
template <typename T> struct wrapper_access {
  using wrapped_type = void;

  [[nodiscard]] static const void* value(const T&) noexcept { return nullptr; }
  [[nodiscard]] static void* mutable_value(T&) noexcept { return nullptr; }
  [[nodiscard]] static bool has_value(const T&) noexcept { return false; }
  static bool reset(T&) noexcept { return false; }
  static bool ensure_value(T&) { return false; }
};

} // namespace caors::traits

namespace caors {

template <typename T> using wrapper_traits = traits::wrapper_traits<T>;
template <typename T> using wrapper_access = traits::wrapper_access<T>;

} // namespace caors

namespace caors::meta {

template <typename T> using wrapper_traits = ::caors::traits::wrapper_traits<T>;
template <typename T> using wrapper_access = ::caors::traits::wrapper_access<T>;

} // namespace caors::meta

#endif
