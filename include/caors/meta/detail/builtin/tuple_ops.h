// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_BUILTIN_TUPLE_OPS_H_
#define CAORS_META_DETAIL_BUILTIN_TUPLE_OPS_H_

#include "caors/core/containers.h"
#include "caors/meta/detail/builtin/capability.h"
#include "caors/traits/container.h"

#include <cstddef>
#include <type_traits>
#include <utility>

namespace caors::meta::detail {

using meta::tuple_element_ops;
using meta::tuple_ops;

template <typename Tuple, std::size_t Index> struct tuple_element_ops_model {
  using type = std::remove_cv_t<std::remove_reference_t<Tuple>>;
  using element_type = typename traits::tuple_traits<type>::template element_type<Index>;

  [[nodiscard]] static meta::type get_value_type() noexcept {
    return make_type_view(descriptor_type_id<element_type>(), descriptor_ptr_of<element_type>());
  }

  [[nodiscard]] static const void* element(const void* storage) noexcept {
    if (storage == nullptr) {
      return nullptr;
    }

    return traits::tuple_traits<type>::template element<Index>(*static_cast<const type*>(storage));
  }

  [[nodiscard]] static void* mutable_element(void* storage) noexcept {
    if (storage == nullptr) {
      return nullptr;
    }

    return traits::tuple_traits<type>::template mutable_element<Index>(
        *static_cast<type*>(storage));
  }

  inline static const tuple_element_ops value{
      &element,
      &mutable_element,
      &get_value_type,
  };
};

template <typename T> struct tuple_ops_model {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;

  template <std::size_t... Indices>
  [[nodiscard]] static tuple_ops make(std::index_sequence<Indices...>) {
    tuple_ops data{};
    data.elements.reserve(sizeof...(Indices));
    (data.elements.push_back(tuple_element_ops_model<type, Indices>::value), ...);
    return data;
  }

  inline static const tuple_ops value =
      make(std::make_index_sequence<traits::tuple_traits<type>::size>{});
};

template <typename T> [[nodiscard]] const tuple_ops* tuple_ops_ptr_of() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (traits::tuple_traits<type>::is_tuple) {
    return tuple_ops_model<type>::value.valid() ? &tuple_ops_model<type>::value : nullptr;
  } else {
    return nullptr;
  }
}

} // namespace caors::meta::detail

#endif
