// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_TYPE_CAPABILITIES_H_
#define CAORS_META_DETAIL_TYPE_CAPABILITIES_H_

#include <cstdint>

namespace caors::objects {

class object;

} // namespace caors::objects

namespace caors::meta::detail {

using indirect_const_value_fn = const void* (*)(const void*) noexcept;
using indirect_mutable_value_fn = void* (*)(void*) noexcept;
using indirect_has_value_fn = bool (*)(const void*) noexcept;
using indirect_reset_fn = bool (*)(void*) noexcept;
using indirect_ensure_value_fn = bool (*)(void*);
using enum_to_i64_fn = std::int64_t (*)(const void*) noexcept;
using enum_to_u64_fn = std::uint64_t (*)(const void*) noexcept;
using enum_from_i64_fn = bool (*)(void*, std::int64_t) noexcept;
using enum_from_u64_fn = bool (*)(void*, std::uint64_t) noexcept;
using meta_object_ref_get_fn = objects::object* (*)(const void*) noexcept;
using meta_object_ref_set_fn = bool (*)(void*, objects::object*) noexcept;

/** @brief 指针或 wrapper 的内部值访问函数表。 */
struct indirect_data {
  indirect_const_value_fn value = nullptr;
  indirect_mutable_value_fn mutable_value = nullptr;
  indirect_has_value_fn has_value = nullptr;
  indirect_reset_fn reset = nullptr;
  indirect_ensure_value_fn ensure_value = nullptr;

  [[nodiscard]] bool valid() const noexcept {
    return value != nullptr || mutable_value != nullptr || has_value != nullptr ||
           reset != nullptr || ensure_value != nullptr;
  }
};

/** @brief 枚举类型和整数之间的转换函数表。 */
struct enum_data {
  enum_to_i64_fn to_i64 = nullptr;
  enum_to_u64_fn to_u64 = nullptr;
  enum_from_i64_fn from_i64 = nullptr;
  enum_from_u64_fn from_u64 = nullptr;
  bool is_signed = false;

  [[nodiscard]] bool valid() const noexcept {
    return to_i64 != nullptr && to_u64 != nullptr && from_i64 != nullptr && from_u64 != nullptr;
  }
};

/** @brief 对象引用 wrapper 和对象指针之间的访问函数表。 */
struct object_ref_data {
  meta_object_ref_get_fn get = nullptr;
  meta_object_ref_set_fn set = nullptr;

  [[nodiscard]] bool valid() const noexcept { return get != nullptr && set != nullptr; }
};

} // namespace caors::meta::detail

#endif
