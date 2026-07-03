// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_CORE_TYPE_GUID_H_
#define CAORS_CORE_TYPE_GUID_H_

#include "caors/core/guid.h"

namespace caors::core {

/**
 * @brief 稳定类型标识。
 *
 * `type_guid` 用于序列化、存档、资源引用和插件边界等需要跨进程、跨版本稳定识别类型的场景。
 * 它和运行期 `meta::type_id` 不同，不应由 `string_name` 的运行期 entry 或哈希值派生。
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

} // namespace caors::core

namespace caors {

using core::type_guid;

} // namespace caors

#endif
