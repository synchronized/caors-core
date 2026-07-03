// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_CONTAINER_REGISTRATION_H_
#define CAORS_META_CONTAINER_REGISTRATION_H_

#include "caors/meta/detail/container_registration_associative.h"
#include "caors/meta/detail/container_registration_pair.h"
#include "caors/meta/detail/container_registration_sequence.h"
#include "caors/meta/detail/container_registration_set.h"
#include "caors/meta/detail/container_registration_tuple.h"
#include "caors/meta/detail/container_registration_variant.h"

namespace caors::meta::detail {
template <typename T> [[nodiscard]] error register_container_data_for(type_id id) {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (traits::sequence_traits<type>::is_sequence) {
    return register_sequence_data_for<type>(id);
  } else if constexpr (traits::set_traits<type>::is_set) {
    return register_set_data_for<type>(id);
  } else if constexpr (traits::associative_traits<type>::is_associative) {
    return register_associative_data_for<type>(id);
  } else if constexpr (traits::pair_traits<type>::is_pair) {
    return register_pair_data_for<type>(id);
  } else if constexpr (traits::tuple_traits<type>::is_tuple) {
    return register_tuple_data_for<type>(id);
  } else if constexpr (traits::variant_traits<type>::is_variant) {
    return register_variant_data_for<type>(id);
  }

  return {};
}

} // namespace caors::meta::detail

#endif
