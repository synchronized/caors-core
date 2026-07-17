// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_REFLECTION_DETAIL_TYPE_WRAPPER_H_
#define CAORS_REFLECTION_DETAIL_TYPE_WRAPPER_H_

#include "caors/meta/detail/reflection_data.h"
#include "caors/reflection/detail/field_wrapper.h"
#include "caors/reflection/detail/method_wrapper.h"

namespace caors::reflection::detail {

using type_ops = meta::detail::reflection_type_ops;
using type_wrapper = meta::detail::reflection_data;

template <typename T> [[nodiscard]] meta::type reflected_type() noexcept {
  return meta::detail::reflected_type<T>();
}

template <typename T> [[nodiscard]] type_ops make_type_ops() noexcept {
  return meta::detail::make_reflection_type_ops<T>();
}

template <typename T>
[[nodiscard]] inline type_wrapper make_type_wrapper(std::initializer_list<field_wrapper> fields) {
  return meta::detail::make_reflection_data<T>(fields);
}

} // namespace caors::reflection::detail

#endif
