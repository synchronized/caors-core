// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_TYPE_ID_H_
#define CAORS_META_TYPE_ID_H_

#include "caors/core/guid.h"
#include "caors/core/string_name.h"

#include <cstdint>
#include <string_view>

namespace caors {

/**
 * @brief 运行期类型标识。
 *
 * 类型标识由 `string_name` 的运行期 id 派生，适合运行期查找和注册表索引。它不是跨版本序列化
 * ID。
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

/**
 * @brief 稳定类型标识。
 *
 * `type_guid` 用于序列化、存档、资源引用和插件边界等需要跨进程、跨版本稳定识别类型的场景。
 * 它和运行期 `type_id` 不同，不应由 `string_name::id()` 派生。
 */
struct type_guid {
  guid value{};

  constexpr type_guid() noexcept = default;
  explicit constexpr type_guid(guid guid_value) noexcept : value(guid_value) {}

  [[nodiscard]] static constexpr type_guid nil() noexcept { return type_guid{}; }
  [[nodiscard]] bool is_nil() const noexcept { return value.is_nil(); }
  [[nodiscard]] explicit operator bool() const noexcept { return !is_nil(); }

  friend constexpr bool operator==(const type_guid& lhs, const type_guid& rhs) noexcept {
    return lhs.value == rhs.value;
  }

  friend constexpr bool operator!=(const type_guid& lhs, const type_guid& rhs) noexcept {
    return !(lhs == rhs);
  }
};

/** @brief 根据类型名生成运行期类型标识。 */
[[nodiscard]] constexpr type_id make_type_id(string_name name) noexcept {
  return type_id{name.id()};
}

/** @brief 根据类型名生成运行期类型标识。 */
[[nodiscard]] inline type_id make_type_id(std::string_view name) {
  return make_type_id(string_name{name});
}

} // namespace caors

#endif
