// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_CONTAINER_REGISTRATION_SEQUENCE_H_
#define CAORS_META_DETAIL_CONTAINER_REGISTRATION_SEQUENCE_H_

#include "caors/meta/registry.h"

namespace caors::meta::detail {
template <typename T> [[nodiscard]] error register_sequence_data_for(type_id id) {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (traits::sequence_traits<type>::is_sequence) {
    using element_type = typename traits::sequence_traits<type>::element_type;

    const type_id value = semantic_type_id<element_type>();
    if (!id || !value) {
      return {};
    }

    meta::detail::sequence_data data{
        value,
        [](const void* storage) noexcept -> std::size_t {
          if (storage == nullptr) {
            return 0;
          }

          return traits::sequence_traits<type>::size(*static_cast<const type*>(storage));
        },
        [](const void* storage, std::size_t index) noexcept -> const void* {
          if (storage == nullptr) {
            return nullptr;
          }

          return traits::sequence_traits<type>::element(*static_cast<const type*>(storage), index);
        },
        [](void* storage, std::size_t index) noexcept -> void* {
          if (storage == nullptr) {
            return nullptr;
          }

          return traits::sequence_traits<type>::mutable_element(*static_cast<type*>(storage),
                                                                index);
        },
        [](void* storage) noexcept {
          if (storage != nullptr) {
            traits::sequence_traits<type>::clear(*static_cast<type*>(storage));
          }
        },
        [](void* storage, std::size_t size) -> bool {
          if (storage == nullptr) {
            return false;
          }

          return traits::sequence_traits<type>::resize(*static_cast<type*>(storage), size);
        },
        [](void* storage, const void* element) -> bool {
          if (storage == nullptr || element == nullptr) {
            return false;
          }

          return traits::sequence_traits<type>::append(*static_cast<type*>(storage),
                                                       *static_cast<const element_type*>(element));
        },
    };

    if constexpr (requires { traits::sequence_traits<type>::read_in_place; }) {
      data.read_in_place = traits::sequence_traits<type>::read_in_place;
    }

    if constexpr (requires { traits::sequence_traits<type>::can_read; }) {
      data.can_read = traits::sequence_traits<type>::can_read;
    }

    const error result = caors::meta::get_registry().emplace<meta::detail::sequence_data>(id, data);
    if (result && result.value() != errc::duplicate_component) {
      return result;
    }
  }

  return {};
}

} // namespace caors::meta::detail

#endif
