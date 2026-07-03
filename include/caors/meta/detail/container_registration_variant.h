// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_CONTAINER_REGISTRATION_VARIANT_H_
#define CAORS_META_DETAIL_CONTAINER_REGISTRATION_VARIANT_H_

#include "caors/meta/registry.h"

namespace caors::meta::detail {
template <typename Variant, std::size_t Index>
[[nodiscard]] variant_alternative_data make_variant_alternative_data() {
  using type = std::remove_cv_t<std::remove_reference_t<Variant>>;
  using alternative_type = typename traits::variant_traits<type>::template alternative_type<Index>;

  return variant_alternative_data{
      semantic_type_id<alternative_type>(),
      [](const void* storage) noexcept -> const void* {
        if (storage == nullptr) {
          return nullptr;
        }

        return traits::variant_traits<type>::template alternative<Index>(
            *static_cast<const type*>(storage));
      },
      [](void* storage) noexcept -> void* {
        if (storage == nullptr) {
          return nullptr;
        }

        return traits::variant_traits<type>::template mutable_alternative<Index>(
            *static_cast<type*>(storage));
      },
      [](void* storage) -> bool {
        if (storage == nullptr) {
          return false;
        }

        return traits::variant_traits<type>::template emplace<Index>(*static_cast<type*>(storage));
      },
  };
}

template <typename Variant, std::size_t... Indices>
[[nodiscard]] meta::detail::variant_data make_variant_data(std::index_sequence<Indices...>) {
  using type = std::remove_cv_t<std::remove_reference_t<Variant>>;

  meta::detail::variant_data data{};
  data.alternatives.reserve(sizeof...(Indices));
  (data.alternatives.push_back(make_variant_alternative_data<type, Indices>()), ...);
  data.index = [](const void* storage) noexcept -> std::size_t {
    if (storage == nullptr) {
      return static_cast<std::size_t>(-1);
    }

    return traits::variant_traits<type>::index(*static_cast<const type*>(storage));
  };
  return data;
}

template <typename T> [[nodiscard]] error register_variant_data_for(type_id id) {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (traits::variant_traits<type>::is_variant) {
    if (!id) {
      return {};
    }

    meta::detail::variant_data data =
        make_variant_data<type>(std::make_index_sequence<traits::variant_traits<type>::size>{});
    if (!data.valid()) {
      return {};
    }

    const error result = caors::meta::get_registry().emplace<meta::detail::variant_data>(id, data);
    if (result && result.value() != errc::duplicate_component) {
      return result;
    }
  }

  return {};
}

} // namespace caors::meta::detail

#endif
