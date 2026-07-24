// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_BUILTIN_OPS_H_
#define CAORS_META_DETAIL_BUILTIN_OPS_H_

#include "caors/meta/type_id.h"
#include "caors/meta/type_ops.h"

#include <type_traits>
#include <variant>

namespace caors::meta {

namespace detail {

class type_record;

template <typename T> [[nodiscard]] const type_record* static_type_record_ptr_of() noexcept;
template <typename T> [[nodiscard]] type_id static_type_id() noexcept {
  return type_id_of_cpp_type<std::remove_cv_t<std::remove_reference_t<T>>>();
}

/** @brief 类型的内置互斥运行期函数表。 */
using builtin_ops =
    std::variant<std::monostate, const meta::scalar_ops*, const meta::enum_ops*,
                 const meta::indirect_ops*, const meta::sequence_ops*, const meta::set_ops*,
                 const meta::associative_ops*, const meta::pair_ops*, const meta::tuple_ops*,
                 const meta::variant_ops*>;

} // namespace detail

} // namespace caors::meta

#endif
