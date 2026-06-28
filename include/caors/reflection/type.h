// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_REFLECTION_TYPE_H_
#define CAORS_REFLECTION_TYPE_H_

#include "caors/reflection/field.h"

#include <initializer_list>
#include <optional>
#include <string_view>
#include <vector>

namespace caors {

/**
 * @brief 类型反射元信息。
 *
 * `reflection_type` 复用基础 `type_info` 表示类型身份，并保存该类型的字段列表。
 */
struct reflection_type {
  type_info info{};
  std::vector<field_info> fields{};

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] std::optional<field_info> find_field(string_name name) const;
  [[nodiscard]] std::optional<field_info> find_field(std::string_view name) const;
};

/** @brief 创建类型反射描述。 */
template <typename T>
[[nodiscard]] reflection_type make_reflection_type(std::string_view name,
                                                   std::initializer_list<field_info> fields,
                                                   type_id parent_id = type_id::invalid()) {
  return reflection_type{
      make_type_info<T>(name, parent_id),
      std::vector<field_info>{fields},
  };
}

/** @brief 创建带稳定 GUID 的类型反射描述。 */
template <typename T>
[[nodiscard]] reflection_type make_reflection_type(std::string_view name, type_guid guid,
                                                   std::initializer_list<field_info> fields,
                                                   type_id parent_id = type_id::invalid()) {
  return reflection_type{
      make_type_info<T>(name, guid, parent_id),
      std::vector<field_info>{fields},
  };
}

} // namespace caors

#endif
