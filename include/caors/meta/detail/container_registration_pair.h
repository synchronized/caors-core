// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_CONTAINER_REGISTRATION_PAIR_H_
#define CAORS_META_DETAIL_CONTAINER_REGISTRATION_PAIR_H_

#include "caors/meta/registry.h"

namespace caors::meta::detail {
template <typename T> [[nodiscard]] error register_pair_data_for(type_id id) {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (traits::pair_traits<type>::is_pair) {
    using first_type = typename traits::pair_traits<type>::first_type;
    using second_type = typename traits::pair_traits<type>::second_type;

    const type_id key = semantic_type_id<first_type>();
    const type_id value = semantic_type_id<second_type>();
    if (!id || !key || !value) {
      return {};
    }

    meta::detail::pair_data data{
        key,
        value,
        [](const void* storage) noexcept -> const void* {
          if (storage == nullptr) {
            return nullptr;
          }

          return traits::pair_traits<type>::first(*static_cast<const type*>(storage));
        },
        [](void* storage) noexcept -> void* {
          if (storage == nullptr) {
            return nullptr;
          }

          return traits::pair_traits<type>::mutable_first(*static_cast<type*>(storage));
        },
        [](const void* storage) noexcept -> const void* {
          if (storage == nullptr) {
            return nullptr;
          }

          return traits::pair_traits<type>::second(*static_cast<const type*>(storage));
        },
        [](void* storage) noexcept -> void* {
          if (storage == nullptr) {
            return nullptr;
          }

          return traits::pair_traits<type>::mutable_second(*static_cast<type*>(storage));
        },
    };

    const error result = caors::meta::get_registry().emplace<meta::detail::pair_data>(id, data);
    if (result && result.value() != errc::duplicate_component) {
      return result;
    }
  }

  return {};
}

} // namespace caors::meta::detail

#endif
