// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_RESOURCES_ID_H_
#define CAORS_RESOURCES_ID_H_

#include "caors/core/guid.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace caors::resources {

/**
 * @brief 稳定资源标识。
 *
 * `resource_guid` 用于跨进程、跨存档和资源数据库稳定识别资源。它不等于运行期对象 id，也不由
 * 资源路径或名称自动派生。
 */
struct resource_guid {
  core::guid value{};

  constexpr resource_guid() noexcept = default;
  explicit constexpr resource_guid(core::guid guid_value) noexcept : value(guid_value) {}

  [[nodiscard]] static constexpr resource_guid nil() noexcept { return resource_guid{}; }

  [[nodiscard]] static std::optional<resource_guid> parse(std::string_view text) {
    auto parsed = core::guid::parse(text);
    if (!parsed) {
      return std::nullopt;
    }

    return resource_guid{*parsed};
  }

  [[nodiscard]] bool is_nil() const noexcept { return value.is_nil(); }
  [[nodiscard]] explicit operator bool() const noexcept { return !is_nil(); }
  [[nodiscard]] std::string string() const { return value.string(); }

  friend constexpr bool operator==(resource_guid lhs, resource_guid rhs) noexcept {
    return lhs.value == rhs.value;
  }

  friend constexpr bool operator!=(resource_guid lhs, resource_guid rhs) noexcept {
    return !(lhs == rhs);
  }
};

/** @brief 资源在当前运行期索引中的绑定版本。0 表示没有有效版本。 */
using resource_generation = std::uint64_t;
inline constexpr resource_generation invalid_resource_generation = 0;

/** @brief `resource_guid` 的哈希函数。 */
struct resource_guid_hash {
  [[nodiscard]] std::size_t operator()(resource_guid guid) const noexcept {
    return core::guid_hash{}(guid.value);
  }
};

} // namespace caors::resources

namespace std {

template <> struct hash<caors::resources::resource_guid> {
  [[nodiscard]] std::size_t operator()(caors::resources::resource_guid guid) const noexcept {
    return caors::resources::resource_guid_hash{}(guid);
  }
};

} // namespace std

namespace caors {

using resource_guid = resources::resource_guid;
using resource_guid_hash = resources::resource_guid_hash;
using resource_generation = resources::resource_generation;
using resources::invalid_resource_generation;

} // namespace caors

#endif
