// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_BUILTIN_TYPE_CAPABILITY_H_
#define CAORS_META_DETAIL_BUILTIN_TYPE_CAPABILITY_H_

#include "caors/meta/detail/container.h"
#include "caors/meta/detail/type_capabilities.h"

#include <variant>

namespace caors::meta::detail {

struct type_descriptor;

template <typename T> [[nodiscard]] const type_descriptor* descriptor_ptr_of() noexcept;
template <typename T> [[nodiscard]] type_id descriptor_type_id() noexcept;

/** @brief 类型的内置互斥运行期能力。 */
using builtin_type_capability =
    std::variant<std::monostate, const enum_ops*, const indirect_ops*, const sequence_ops*,
                 const set_ops*, const associative_ops*, const pair_ops*, const tuple_ops*,
                 const variant_ops*>;

} // namespace caors::meta::detail

#endif
