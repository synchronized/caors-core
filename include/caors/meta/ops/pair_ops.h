// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_OPS_PAIR_OPS_H_
#define CAORS_META_OPS_PAIR_OPS_H_

#include "caors/meta/type.h"

namespace caors::meta {

/** @brief 二元组运行期访问操作表。 */
struct pair_ops {
  using get_key_type_fn = type (*)() noexcept;
  using get_value_type_fn = type (*)() noexcept;
  using const_value_fn = const void* (*)(const void*) noexcept;
  using mutable_value_fn = void* (*)(void*) noexcept;

  const_value_fn first = nullptr;
  mutable_value_fn mutable_first = nullptr;
  const_value_fn second = nullptr;
  mutable_value_fn mutable_second = nullptr;
  get_key_type_fn get_key_type = nullptr;
  get_value_type_fn get_value_type = nullptr;

  [[nodiscard]] bool valid() const noexcept {
    return get_key_type != nullptr && get_value_type != nullptr && first != nullptr &&
           second != nullptr;
  }
};

} // namespace caors::meta

#endif
