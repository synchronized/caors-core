// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_BUILTIN_VARIANT_OPS_H_
#define CAORS_META_DETAIL_BUILTIN_VARIANT_OPS_H_

#include "caors/meta/detail/builtin/capability.h"
#include "caors/traits/container.h"

#include <cstddef>
#include <type_traits>
#include <utility>

namespace caors::meta::detail {

template <typename Variant, std::size_t Index> struct variant_alternative_ops_model {
  using type = std::remove_cv_t<std::remove_reference_t<Variant>>;
  using alternative_type = typename traits::variant_traits<type>::template alternative_type<Index>;

  inline static const variant_alternative_ops value{
      descriptor_type_id<alternative_type>(),
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
      [](void* storage, memory::memory_resource* resource) -> bool {
        if (storage == nullptr) {
          return false;
        }

        if constexpr (requires(type& values, memory::memory_resource* emplace_resource) {
                        traits::variant_traits<type>::template emplace<Index>(values,
                                                                              emplace_resource);
                      }) {
          return traits::variant_traits<type>::template emplace<Index>(*static_cast<type*>(storage),
                                                                       resource);
        } else {
          (void)resource;
          return traits::variant_traits<type>::template emplace<Index>(
              *static_cast<type*>(storage));
        }
      },
      descriptor_ptr_of<alternative_type>(),
  };
};

template <typename T> struct variant_ops_model {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;

  template <std::size_t... Indices>
  [[nodiscard]] static variant_ops make(std::index_sequence<Indices...>) {
    variant_ops data{};
    data.alternatives.reserve(sizeof...(Indices));
    (data.alternatives.push_back(variant_alternative_ops_model<type, Indices>::value), ...);
    data.index = [](const void* storage) noexcept -> std::size_t {
      if (storage == nullptr) {
        return static_cast<std::size_t>(-1);
      }

      return traits::variant_traits<type>::index(*static_cast<const type*>(storage));
    };
    return data;
  }

  inline static const variant_ops value =
      make(std::make_index_sequence<traits::variant_traits<type>::size>{});
};

template <typename T> [[nodiscard]] const variant_ops* variant_ops_ptr_of() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (traits::variant_traits<type>::is_variant) {
    return variant_ops_model<type>::value.valid() ? &variant_ops_model<type>::value : nullptr;
  } else {
    return nullptr;
  }
}

} // namespace caors::meta::detail

#endif
