// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_CONTAINER_REGISTRATION_TUPLE_H_
#define CAORS_META_DETAIL_CONTAINER_REGISTRATION_TUPLE_H_

#include "caors/meta/registry.h"

namespace caors::meta::detail {
template <typename Tuple, std::size_t Index>
[[nodiscard]] tuple_element_data make_tuple_element_data() {
  using type = std::remove_cv_t<std::remove_reference_t<Tuple>>;
  using element_type = typename traits::tuple_traits<type>::template element_type<Index>;

  return tuple_element_data{
      semantic_type_id<element_type>(),
      [](const void* storage) noexcept -> const void* {
        if (storage == nullptr) {
          return nullptr;
        }

        return traits::tuple_traits<type>::template element<Index>(
            *static_cast<const type*>(storage));
      },
      [](void* storage) noexcept -> void* {
        if (storage == nullptr) {
          return nullptr;
        }

        return traits::tuple_traits<type>::template mutable_element<Index>(
            *static_cast<type*>(storage));
      },
  };
}

template <typename Tuple, std::size_t... Indices>
[[nodiscard]] meta::detail::tuple_data make_tuple_data(std::index_sequence<Indices...>) {
  meta::detail::tuple_data data{};
  data.elements.reserve(sizeof...(Indices));
  (data.elements.push_back(make_tuple_element_data<Tuple, Indices>()), ...);
  return data;
}

template <typename T> [[nodiscard]] error register_tuple_data_for(type_id id) {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (traits::tuple_traits<type>::is_tuple) {
    if (!id) {
      return {};
    }

    meta::detail::tuple_data data =
        make_tuple_data<type>(std::make_index_sequence<traits::tuple_traits<type>::size>{});
    if (!data.valid()) {
      return {};
    }

    const error result = caors::meta::get_registry().emplace<meta::detail::tuple_data>(id, data);
    if (result && result.value() != errc::duplicate_component) {
      return result;
    }
  }

  return {};
}

} // namespace caors::meta::detail

#endif
