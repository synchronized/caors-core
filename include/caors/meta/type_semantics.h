// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_TYPE_SEMANTICS_H_
#define CAORS_META_TYPE_SEMANTICS_H_

#include "caors/traits/wrapper.h"

#include <type_traits>

namespace caors::meta::detail {

template <typename T>
inline constexpr bool is_supported_wrapper_v =
    traits::wrapper_traits<std::remove_cv_t<T>>::is_wrapper;

template <typename T>
inline constexpr bool is_nested_type_indirection_v =
    std::is_pointer_v<std::remove_cv_t<T>> || is_supported_wrapper_v<T>;

} // namespace caors::meta::detail

#endif
