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
 * @brief 类型反射底层数据。
 *
 * 该类型只保存反射组件自己的数据，例如字段列表；类型身份由 `meta_type_data` 和 `meta_type`
 * 统一表达。外部查询优先使用 `reflection_type` 只读视图。
 */
struct reflection_type_data {
  std::vector<reflection_field_data> fields{};

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] std::optional<reflection_field_data> find_field(string_name name) const;
  [[nodiscard]] std::optional<reflection_field_data> find_field(std::string_view name) const;
};

/** @brief 创建类型反射底层数据。 */
[[nodiscard]] inline reflection_type_data
make_reflection_type_data(std::initializer_list<reflection_field_data> fields) {
  return reflection_type_data{std::vector<reflection_field_data>{fields}};
}

} // namespace caors

#endif
