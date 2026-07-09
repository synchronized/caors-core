// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_TYPE_SCHEMA_H_
#define CAORS_SERIALIZATION_TYPE_SCHEMA_H_

#include "caors/core/containers.h"
#include "caors/core/string_name.h"

#include <cstdint>
#include <initializer_list>
#include <string_view>

namespace caors::serialization {

/** @brief 序列化后端能力。 */
enum class format_features : std::uint32_t {
  none = 0,
  named_fields = 1U << 0,
  numeric_fields = 1U << 1,
  ordered_fields = 1U << 2,
  self_describing = 1U << 3,
  schema_required = 1U << 4,
};

[[nodiscard]] constexpr format_features operator|(format_features lhs,
                                                  format_features rhs) noexcept {
  return static_cast<format_features>(static_cast<std::uint32_t>(lhs) |
                                      static_cast<std::uint32_t>(rhs));
}

[[nodiscard]] constexpr format_features operator&(format_features lhs,
                                                  format_features rhs) noexcept {
  return static_cast<format_features>(static_cast<std::uint32_t>(lhs) &
                                      static_cast<std::uint32_t>(rhs));
}

[[nodiscard]] constexpr bool has_feature(format_features features,
                                         format_features feature) noexcept {
  return (features & feature) != format_features::none;
}

/** @brief 单个字段的序列化配置。 */
struct field_data {
  string_name field_name{};
  string_name serialized_name{};
  std::uint32_t field_id = 0;
  std::uint32_t version_added = 0;
  std::uint32_t version_removed = 0;
  bool required = false;
  bool skipped = false;

  [[nodiscard]] constexpr bool valid() const noexcept { return !field_name.empty(); }
};

/**
 * @brief 序列化类型元信息。
 *
 * 字段配置按字段名引用反射字段。注册序列化组件时不要求反射字段已经注册，实际序列化时再解析
 * 字段配置和反射字段。
 */
struct type_schema {
  vector<field_data> fields{};

  [[nodiscard]] bool valid() const noexcept {
    for (const field_data& field : fields) {
      if (!field.valid()) {
        return false;
      }
    }

    return true;
  }
};

/** @brief 创建字段序列化配置。 */
[[nodiscard]] inline field_data make_field(std::string_view field_name,
                                           std::string_view serialized_name = {},
                                           bool skipped = false, std::uint32_t field_id = 0) {
  return field_data{
      string_name{field_name},
      serialized_name.empty() ? string_name{field_name} : string_name{serialized_name},
      field_id,
      0,
      0,
      false,
      skipped,
  };
}

/** @brief 创建类型序列化配置。 */
[[nodiscard]] inline type_schema make_type_schema(std::initializer_list<field_data> fields) {
  return type_schema{vector<field_data>{fields}};
}

} // namespace caors::serialization

#endif
