// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_DETAIL_TYPE_KEY_H_
#define CAORS_DETAIL_TYPE_KEY_H_

#include "caors/traits/type_name.h"

#include <type_traits>

namespace caors::detail {

using cpp_type_key = const void*;

template <typename T> struct cpp_type_key_storage {
  inline static const int key = 0;
};

template <typename T> [[nodiscard]] cpp_type_key cpp_type_key_of() noexcept {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  return &cpp_type_key_storage<type>::key;
}

template <typename T, typename = void> struct has_type_name : std::false_type {};

template <typename T>
struct has_type_name<
    T,
    std::void_t<decltype(traits::type_name<std::remove_cv_t<std::remove_reference_t<T>>>::value)>>
    : std::true_type {};

template <typename T> inline constexpr bool has_type_name_v = has_type_name<T>::value;

} // namespace caors::detail

#endif
