// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_TYPE_DESCRIPTOR_H_
#define CAORS_META_DETAIL_TYPE_DESCRIPTOR_H_

#include "caors/meta/type_descriptor.h"

namespace caors::meta::detail {

template <typename T> [[nodiscard]] const meta::type_descriptor* descriptor_ptr_of() noexcept;
template <typename T> [[nodiscard]] type_id descriptor_type_id() noexcept {
  return meta::descriptor_type_id<T>();
}
[[nodiscard]] CAORS_API error
ensure_type_descriptor_registered(const meta::type_descriptor& descriptor);

} // namespace caors::meta::detail

#endif
