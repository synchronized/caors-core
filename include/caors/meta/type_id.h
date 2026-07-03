// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_TYPE_ID_H_
#define CAORS_META_TYPE_ID_H_

#include "caors/core/type_guid.h"

#include <cstdint>

namespace caors::meta {

/**
 * @brief 运行期类型标识。
 *
 * 类型标识由全局 `meta::registry` 分配，适合当前进程内的注册表索引。它不是类型名 hash，也不是
 * 跨版本序列化 ID。
 */
class type_id {
public:
  constexpr type_id() noexcept = default;
  explicit constexpr type_id(std::uint64_t value) noexcept : value_(value) {}

  [[nodiscard]] static constexpr type_id invalid() noexcept { return type_id{}; }
  [[nodiscard]] constexpr std::uint64_t value() const noexcept { return value_; }
  [[nodiscard]] explicit constexpr operator bool() const noexcept { return value_ != 0; }

  friend constexpr bool operator==(type_id lhs, type_id rhs) noexcept {
    return lhs.value_ == rhs.value_;
  }

  friend constexpr bool operator!=(type_id lhs, type_id rhs) noexcept { return !(lhs == rhs); }

private:
  std::uint64_t value_ = 0;
};

using type_guid = core::type_guid;

} // namespace caors::meta

namespace caors {

using type_id = meta::type_id;

} // namespace caors

#endif
