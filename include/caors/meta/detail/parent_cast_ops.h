// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_PARENT_CAST_OPS_H_
#define CAORS_META_DETAIL_PARENT_CAST_OPS_H_

#include "caors/meta/type_id.h"

namespace caors::meta::detail {

using parent_const_upcast_fn = const void* (*)(const void*) noexcept;
using parent_mutable_upcast_fn = void* (*)(void*) noexcept;

/** @brief 子类型到直接父类型的安全 upcast 函数表。 */
struct parent_cast_ops {
  type_id parent{};
  parent_const_upcast_fn const_upcast = nullptr;
  parent_mutable_upcast_fn mutable_upcast = nullptr;

  [[nodiscard]] bool valid() const noexcept {
    return parent && const_upcast != nullptr && mutable_upcast != nullptr;
  }
};

} // namespace caors::meta::detail

#endif
