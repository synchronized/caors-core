// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_TYPE_DESCRIPTOR_MODEL_H_
#define CAORS_META_TYPE_DESCRIPTOR_MODEL_H_

#include "caors/meta/detail/type_descriptor_model.h"
#include "caors/meta/type_descriptor.h"

#include <type_traits>

namespace caors::meta {

/** @brief 获取指定 C++ 类型的 descriptor；类型不要求已经显式注册。 */
template <typename T> [[nodiscard]] const type_descriptor* descriptor_of() noexcept {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  return detail::descriptor_ptr_of<type>();
}

/** @brief 获取指定 C++ 类型的 descriptor id；类型不要求已经显式注册。 */
template <typename T> [[nodiscard]] type_id descriptor_id_of() noexcept {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  return descriptor_type_id<type>();
}

} // namespace caors::meta

#endif
