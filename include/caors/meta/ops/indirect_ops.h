// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_OPS_INDIRECT_OPS_H_
#define CAORS_META_OPS_INDIRECT_OPS_H_

#include "caors/memory/resource.h"
#include "caors/meta/type.h"
#include "caors/meta/type_id.h"
#include "caors/traits/wrapper.h"

namespace caors::meta {

/** @brief 指针或 wrapper 的内部值访问函数表。 */
struct indirect_ops {
  using get_value_type_fn = type (*)() noexcept;
  using const_value_fn = const void* (*)(const void*) noexcept;
  using mutable_value_fn = void* (*)(void*) noexcept;
  using dynamic_type_fn = type_id (*)(const void*) noexcept;
  using dynamic_value_fn = const void* (*)(const void*) noexcept;
  using mutable_dynamic_value_fn = void* (*)(void*) noexcept;
  using has_value_fn = bool (*)(const void*) noexcept;
  using reset_fn = bool (*)(void*) noexcept;
  using ensure_value_fn = bool (*)(void*, memory::memory_resource*);
  using emplace_value_fn = bool (*)(void*, void*);
  using set_value_fn = bool (*)(void*, void*) noexcept;
  using set_value_with_context_fn = bool (*)(void*, void*,
                                             const traits::wrapper_set_context&) noexcept;

  const_value_fn value = nullptr;
  mutable_value_fn mutable_value = nullptr;
  dynamic_type_fn dynamic_type = nullptr;
  dynamic_value_fn dynamic_value = nullptr;
  mutable_dynamic_value_fn mutable_dynamic_value = nullptr;
  has_value_fn has_value = nullptr;
  reset_fn reset = nullptr;
  ensure_value_fn ensure_value = nullptr;
  emplace_value_fn emplace_value = nullptr;
  set_value_fn set_value = nullptr;
  set_value_with_context_fn set_value_with_context = nullptr;
  get_value_type_fn get_value_type = nullptr;

  [[nodiscard]] bool valid() const noexcept {
    return value != nullptr || mutable_value != nullptr || has_value != nullptr ||
           reset != nullptr || ensure_value != nullptr || emplace_value != nullptr ||
           dynamic_type != nullptr || dynamic_value != nullptr ||
           mutable_dynamic_value != nullptr || set_value != nullptr ||
           set_value_with_context != nullptr;
  }
};

} // namespace caors::meta

#endif
