// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_BUILTIN_OPS_MODEL_H_
#define CAORS_META_DETAIL_BUILTIN_OPS_MODEL_H_

#include "caors/meta/detail/builtin/associative_ops.h"
#include "caors/meta/detail/builtin/enum_ops.h"
#include "caors/meta/detail/builtin/indirect_ops.h"
#include "caors/meta/detail/builtin/pair_ops.h"
#include "caors/meta/detail/builtin/sequence_ops.h"
#include "caors/meta/detail/builtin/set_ops.h"
#include "caors/meta/detail/builtin/tuple_ops.h"
#include "caors/meta/detail/builtin/variant_ops.h"

#include "caors/traits/container.h"
#include "caors/traits/wrapper.h"

#include <type_traits>
#include <variant>

namespace caors::meta::detail {

template <typename T> [[nodiscard]] builtin_ops builtin_ops_of() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;

  if constexpr (std::is_pointer_v<type>) {
    return indirect_ops_ptr_of<type>();
  } else if constexpr (traits::sequence_traits<type>::is_sequence) {
    return sequence_ops_ptr_of<type>();
  } else if constexpr (traits::set_traits<type>::is_set) {
    return set_ops_ptr_of<type>();
  } else if constexpr (traits::associative_traits<type>::is_associative) {
    return associative_ops_ptr_of<type>();
  } else if constexpr (traits::pair_traits<type>::is_pair) {
    return pair_ops_ptr_of<type>();
  } else if constexpr (traits::tuple_traits<type>::is_tuple) {
    return tuple_ops_ptr_of<type>();
  } else if constexpr (traits::variant_traits<type>::is_variant) {
    return variant_ops_ptr_of<type>();
  } else if constexpr (traits::wrapper_traits<type>::is_wrapper) {
    return indirect_ops_ptr_of<type>();
  } else if constexpr (std::is_enum_v<type>) {
    return enum_ops_ptr_of<type>();
  } else {
    return std::monostate{};
  }
}

} // namespace caors::meta::detail

#endif
