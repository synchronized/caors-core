// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_BUILTIN_PAIR_OPS_H_
#define CAORS_META_DETAIL_BUILTIN_PAIR_OPS_H_

#include "caors/meta/detail/builtin/capability.h"
#include "caors/traits/container.h"

#include <type_traits>

namespace caors::meta::detail {

using pair_const_value_fn = const void* (*)(const void*) noexcept;
using pair_mutable_value_fn = void* (*)(void*) noexcept;

/** @brief 二元组运行期访问操作表。 */
struct pair_ops {
  type_id key_type{};
  type_id value_type{};
  pair_const_value_fn first = nullptr;
  pair_mutable_value_fn mutable_first = nullptr;
  pair_const_value_fn second = nullptr;
  pair_mutable_value_fn mutable_second = nullptr;
  const type_descriptor* key_descriptor = nullptr;
  const type_descriptor* value_descriptor = nullptr;

  [[nodiscard]] bool valid() const noexcept {
    return key_type && value_type && first != nullptr && second != nullptr;
  }
};

template <typename T> struct pair_ops_model {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  using first_type = typename traits::pair_traits<type>::first_type;
  using second_type = typename traits::pair_traits<type>::second_type;

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
        descriptor_type_id<first_type>(),
        descriptor_type_id<second_type>(),
        &first,
        &mutable_first,
        &second,
        &mutable_second,
    };

    result.key_descriptor = descriptor_ptr_of<first_type>();
    result.value_descriptor = descriptor_ptr_of<second_type>();
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
