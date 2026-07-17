// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_OPS_ENUM_OPS_H_
#define CAORS_META_OPS_ENUM_OPS_H_

#include <cstdint>

namespace caors::meta {

/** @brief 枚举类型和整数之间的转换函数表。 */
struct enum_ops {
  using to_i64_fn = std::int64_t (*)(const void*) noexcept;
  using to_u64_fn = std::uint64_t (*)(const void*) noexcept;
  using from_i64_fn = bool (*)(void*, std::int64_t) noexcept;
  using from_u64_fn = bool (*)(void*, std::uint64_t) noexcept;

  to_i64_fn to_i64 = nullptr;
  to_u64_fn to_u64 = nullptr;
  from_i64_fn from_i64 = nullptr;
  from_u64_fn from_u64 = nullptr;
  bool is_signed = false;

  [[nodiscard]] bool valid() const noexcept {
    return to_i64 != nullptr && to_u64 != nullptr && from_i64 != nullptr && from_u64 != nullptr;
  }
};

} // namespace caors::meta

#endif
