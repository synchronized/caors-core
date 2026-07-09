// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_BUILTIN_PAIR_OPS_H_
#define CAORS_META_DETAIL_BUILTIN_PAIR_OPS_H_

#include "caors/meta/detail/builtin/capability.h"
#include "caors/traits/container.h"

#include <type_traits>

namespace caors::meta::detail {

template <typename T> struct pair_ops_model {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  using first_type = typename traits::pair_traits<type>::first_type;
  using second_type = typename traits::pair_traits<type>::second_type;

  inline static const pair_ops value = [] {
    pair_ops result{
        descriptor_type_id<first_type>(),
        descriptor_type_id<second_type>(),
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

    result.key_descriptor = descriptor_ptr_of<first_type>();
    result.value_descriptor = descriptor_ptr_of<second_type>();
    return result;
  }();
};

template <typename T> [[nodiscard]] const pair_ops* pair_ops_ptr_of() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (traits::pair_traits<type>::is_pair) {
    return pair_ops_model<type>::value.valid() ? &pair_ops_model<type>::value : nullptr;
  } else {
    return nullptr;
  }
}

} // namespace caors::meta::detail

#endif
