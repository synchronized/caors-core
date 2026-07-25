// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_PLUGINS_SEMANTIC_VERSION_H_
#define CAORS_PLUGINS_SEMANTIC_VERSION_H_

#include "caors/export.h"
#include "caors/result.h"

#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace caors::plugins {

/** @brief 不包含预发布标签的三段式语义版本。 */
struct semantic_version {
  std::uint32_t major = 0;
  std::uint32_t minor = 0;
  std::uint32_t patch = 0;

  [[nodiscard]] CAORS_API static result<semantic_version> parse(std::string_view text);
  [[nodiscard]] CAORS_API std::string string() const;

  auto operator<=>(const semantic_version&) const = default;
};

/** @brief 由可选上下界组成的语义版本范围。 */
struct version_range {
  std::optional<semantic_version> minimum{};
  std::optional<semantic_version> maximum{};
  bool minimum_inclusive = true;
  bool maximum_inclusive = true;

  /** @brief 解析 `*`、精确版本或由空格分隔的比较表达式。 */
  [[nodiscard]] CAORS_API static result<version_range> parse(std::string_view text);
  [[nodiscard]] CAORS_API bool contains(semantic_version value) const noexcept;
};

} // namespace caors::plugins

#endif
