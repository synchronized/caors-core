// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_BUILTIN_PAIR_OPS_MODEL_H_
#define CAORS_META_DETAIL_BUILTIN_PAIR_OPS_MODEL_H_

#include "caors/meta/detail/builtin/ops.h"
#include "caors/meta/detail/type_view.h"
#include "caors/traits/container.h"

#include <type_traits>

namespace caors::meta::detail {

using meta::pair_ops;

template <typename T> struct pair_ops_model {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  using first_type = typename traits::pair_traits<type>::first_type;
  using second_type = typename traits::pair_traits<type>::second_type;

  [[nodiscard]] static meta::type get_key_type() noexcept {
    return make_type_view(descriptor_type_id<first_type>(), static_type_data_ptr_of<first_type>());
  }

  [[nodiscard]] static meta::type get_value_type() noexcept {
    return make_type_view(descriptor_type_id<second_type>(),
                          static_type_data_ptr_of<second_type>());
  }

  [[nodiscard]] static const void* first(const void* storage) noexcept {
    if (storage == nullptr) {
      return nullptr;
    }

    return traits::pair_traits<type>::first(*static_cast<const type*>(storage));
  }

  [[nodiscard]] static void* mutable_first(void* storage) noexcept {
    if (storage == nullptr) {
      return nullptr;
    }

    return traits::pair_traits<type>::mutable_first(*static_cast<type*>(storage));
  }

  [[nodiscard]] static const void* second(const void* storage) noexcept {
    if (storage == nullptr) {
      return nullptr;
    }

    return traits::pair_traits<type>::second(*static_cast<const type*>(storage));
  }

  [[nodiscard]] static void* mutable_second(void* storage) noexcept {
    if (storage == nullptr) {
      return nullptr;
    }

    return traits::pair_traits<type>::mutable_second(*static_cast<type*>(storage));
  }

  [[nodiscard]] static pair_ops make() {
    pair_ops result{
        &first,
        &mutable_first,
        &second,
        &mutable_second,
    };

    result.get_key_type = &get_key_type;
    result.get_value_type = &get_value_type;
    return result;
  }

  inline static const pair_ops value = make();
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
