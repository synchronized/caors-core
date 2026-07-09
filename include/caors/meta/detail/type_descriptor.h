// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_TYPE_DESCRIPTOR_H_
#define CAORS_META_DETAIL_TYPE_DESCRIPTOR_H_

#include "caors/meta/type_descriptor.h"

namespace caors::meta::detail {

using meta::descriptor_capability;
using meta::descriptor_indirect;
using meta::descriptor_key_type;
using meta::descriptor_kind;
using meta::descriptor_nullable;
using meta::descriptor_static_capability;
using meta::descriptor_value_type;
using meta::descriptor_wrapper_semantics;
using meta::type_descriptor;
using meta::type_layout;
using meta::type_semantics;
using meta::value_ops;
using meta::value_traits;

template <typename T> [[nodiscard]] const meta::type_descriptor* descriptor_ptr_of() noexcept;
template <typename T> [[nodiscard]] type_id descriptor_type_id() noexcept {
  return meta::descriptor_type_id<T>();
}
[[nodiscard]] CAORS_API error
ensure_type_descriptor_registered(const meta::type_descriptor& descriptor);

} // namespace caors::meta::detail

#endif
