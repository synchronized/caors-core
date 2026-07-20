// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_OPS_SCALAR_OPS_H_
#define CAORS_META_OPS_SCALAR_OPS_H_

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace caors::meta {

/** @brief 可直接按序列化标量语义处理的 C++ 类型分类。 */
enum class scalar_kind {
  none,
  boolean,
  character,
  signed_integer,
  unsigned_integer,
  floating_point,
  string,
};

/** @brief 标量类型的基础类型擦除转换函数表。 */
struct scalar_ops {
  using to_bool_fn = bool (*)(const void*, bool&) noexcept;
  using to_i64_fn = bool (*)(const void*, std::int64_t&) noexcept;
  using to_u64_fn = bool (*)(const void*, std::uint64_t&) noexcept;
  using to_f64_fn = bool (*)(const void*, double&) noexcept;
  using to_string_fn = bool (*)(const void*, std::string_view&) noexcept;
  using from_bool_fn = bool (*)(void*, bool) noexcept;
  using from_i64_fn = bool (*)(void*, std::int64_t) noexcept;
  using from_u64_fn = bool (*)(void*, std::uint64_t) noexcept;
  using from_f64_fn = bool (*)(void*, double) noexcept;
  using from_string_fn = bool (*)(void*, std::string_view);

  scalar_kind kind = scalar_kind::none;
  std::size_t size = 0;
  bool is_signed = false;
  bool string_borrows_input = false;
  to_bool_fn to_bool = nullptr;
  to_i64_fn to_i64 = nullptr;
  to_u64_fn to_u64 = nullptr;
  to_f64_fn to_f64 = nullptr;
  to_string_fn to_string = nullptr;
  from_bool_fn from_bool = nullptr;
  from_i64_fn from_i64 = nullptr;
  from_u64_fn from_u64 = nullptr;
  from_f64_fn from_f64 = nullptr;
  from_string_fn from_string = nullptr;

  [[nodiscard]] bool valid() const noexcept { return kind != scalar_kind::none; }
};

} // namespace caors::meta

#endif
