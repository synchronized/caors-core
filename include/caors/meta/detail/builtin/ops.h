// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_BUILTIN_OPS_H_
#define CAORS_META_DETAIL_BUILTIN_OPS_H_

#include "caors/meta/type_ops.h"

#include <variant>

namespace caors::meta {

namespace detail {

struct type_descriptor;
class type_data;

template <typename T> [[nodiscard]] const type_descriptor* descriptor_ptr_of() noexcept;
template <typename T> [[nodiscard]] const type_data* static_type_data_ptr_of() noexcept;
template <typename T> [[nodiscard]] type_id descriptor_type_id() noexcept;

/** @brief 类型的内置互斥运行期函数表。 */
using builtin_ops =
    std::variant<std::monostate, const meta::scalar_ops*, const meta::enum_ops*,
                 const meta::indirect_ops*, const meta::sequence_ops*, const meta::set_ops*,
                 const meta::associative_ops*, const meta::pair_ops*, const meta::tuple_ops*,
                 const meta::variant_ops*>;

} // namespace detail

} // namespace caors::meta

#endif
