// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_OBJECT_WRAPPER_TRAITS_H_
#define CAORS_OBJECT_WRAPPER_TRAITS_H_

#include "caors/meta/wrapper.h"
#include "caors/object.h"

#include <type_traits>

namespace caors {

/** @brief `object_ref<T>` 的 wrapper 类型语义适配。 */
template <typename T> struct wrapper_traits<object_ref<T>> {
  static constexpr bool is_wrapper = true;
  static constexpr bool nullable = true;
  static constexpr wrapper_semantics semantics = wrapper_semantics::object_ref;
  using wrapped_type = std::remove_cv_t<T>;
};

/** @brief `object_ref<T>` 的内部值访问适配。 */
template <typename T> struct wrapper_access<object_ref<T>> {
  using wrapped_type = std::remove_cv_t<T>;

  [[nodiscard]] static const wrapped_type* value(const object_ref<T>& source) noexcept {
    return source.get();
  }

  [[nodiscard]] static wrapped_type* mutable_value(object_ref<T>& source) noexcept {
    return source.get();
  }

  [[nodiscard]] static bool has_value(const object_ref<T>& source) noexcept {
    return source.get() != nullptr;
  }

  static bool reset(object_ref<T>& source) noexcept {
    source.reset();
    return true;
  }

  [[nodiscard]] static object* object_value(const object_ref<T>& source) noexcept {
    return source.get();
  }

  static bool set_object(object_ref<T>& source, object* instance) noexcept {
    if (instance == nullptr) {
      source.reset();
      return true;
    }

    auto* typed = dynamic_cast<wrapped_type*>(instance);
    if (typed == nullptr) {
      return false;
    }

    source.reset(typed);
    return true;
  }
};

} // namespace caors

#endif
