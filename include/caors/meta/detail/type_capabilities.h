// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_TYPE_CAPABILITIES_H_
#define CAORS_META_DETAIL_TYPE_CAPABILITIES_H_

#include "caors/memory/resource.h"
#include "caors/meta/type_id.h"
#include "caors/traits/wrapper.h"

#include <cstdint>

namespace caors::meta::detail {

struct type_descriptor;

using indirect_const_value_fn = const void* (*)(const void*) noexcept;
using indirect_mutable_value_fn = void* (*)(void*) noexcept;
using indirect_dynamic_type_fn = type_id (*)(const void*) noexcept;
using indirect_dynamic_value_fn = const void* (*)(const void*) noexcept;
using indirect_mutable_dynamic_value_fn = void* (*)(void*) noexcept;
using indirect_has_value_fn = bool (*)(const void*) noexcept;
using indirect_reset_fn = bool (*)(void*) noexcept;
using indirect_ensure_value_fn = bool (*)(void*, memory::memory_resource*);
using indirect_set_value_fn = bool (*)(void*, void*) noexcept;
using indirect_set_value_with_context_fn = bool (*)(void*, void*,
                                                    const traits::wrapper_set_context&) noexcept;
using enum_to_i64_fn = std::int64_t (*)(const void*) noexcept;
using enum_to_u64_fn = std::uint64_t (*)(const void*) noexcept;
using enum_from_i64_fn = bool (*)(void*, std::int64_t) noexcept;
using enum_from_u64_fn = bool (*)(void*, std::uint64_t) noexcept;
using parent_const_upcast_fn = const void* (*)(const void*) noexcept;
using parent_mutable_upcast_fn = void* (*)(void*) noexcept;

/** @brief 指针或 wrapper 的内部值访问函数表。 */
struct indirect_ops {
  indirect_const_value_fn value = nullptr;
  indirect_mutable_value_fn mutable_value = nullptr;
  indirect_dynamic_type_fn dynamic_type = nullptr;
  indirect_dynamic_value_fn dynamic_value = nullptr;
  indirect_mutable_dynamic_value_fn mutable_dynamic_value = nullptr;
  indirect_has_value_fn has_value = nullptr;
  indirect_reset_fn reset = nullptr;
  indirect_ensure_value_fn ensure_value = nullptr;
  indirect_set_value_fn set_value = nullptr;
  indirect_set_value_with_context_fn set_value_with_context = nullptr;
  const type_descriptor* value_descriptor = nullptr;

  [[nodiscard]] bool valid() const noexcept {
    return value != nullptr || mutable_value != nullptr || has_value != nullptr ||
           reset != nullptr || ensure_value != nullptr || dynamic_type != nullptr ||
           dynamic_value != nullptr || mutable_dynamic_value != nullptr || set_value != nullptr ||
           set_value_with_context != nullptr;
  }
};

/** @brief 枚举类型和整数之间的转换函数表。 */
struct enum_ops {
  enum_to_i64_fn to_i64 = nullptr;
  enum_to_u64_fn to_u64 = nullptr;
  enum_from_i64_fn from_i64 = nullptr;
  enum_from_u64_fn from_u64 = nullptr;
  bool is_signed = false;

  [[nodiscard]] bool valid() const noexcept {
    return to_i64 != nullptr && to_u64 != nullptr && from_i64 != nullptr && from_u64 != nullptr;
  }
};

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
