// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_TRAITS_OBJECT_REF_H_
#define CAORS_TRAITS_OBJECT_REF_H_

#include "caors/object.h"
#include "caors/traits/wrapper.h"

#include <type_traits>

namespace caors::traits {

/** @brief `objects::ref<T>` 的 wrapper 类型语义适配。 */
template <typename T> struct wrapper_traits<objects::ref<T>> {
  static constexpr bool is_wrapper = true;
  static constexpr bool nullable = true;
  static constexpr wrapper_semantics semantics = wrapper_semantics::object_ref;
  using wrapped_type = std::remove_cv_t<T>;
};

/** @brief `objects::ref<T>` 的内部值访问适配。 */
template <typename T> struct wrapper_access<objects::ref<T>> {
  using wrapped_type = std::remove_cv_t<T>;

  [[nodiscard]] static const wrapped_type* value(const objects::ref<T>& source) noexcept {
    return source.get();
  }

  [[nodiscard]] static wrapped_type* mutable_value(objects::ref<T>& source) noexcept {
    return source.get();
  }

  [[nodiscard]] static bool has_value(const objects::ref<T>& source) noexcept {
    return source.get() != nullptr;
  }

  static bool reset(objects::ref<T>& source) noexcept {
    source.reset();
    return true;
  }

  [[nodiscard]] static object* object_value(const objects::ref<T>& source) noexcept {
    return source.get();
  }

  static bool set_object(objects::ref<T>& source, object* instance) noexcept {
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

} // namespace caors::traits

#endif
