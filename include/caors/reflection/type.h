// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_REFLECTION_TYPE_H_
#define CAORS_REFLECTION_TYPE_H_

#include "caors/core/containers.h"
#include "caors/reflection/field.h"

#include <initializer_list>
#include <optional>
#include <string_view>

namespace caors::reflection::detail {

/**
 * @brief 类型反射底层数据。
 *
 * 该类型只保存反射组件自己的数据，例如字段列表；类型身份由 `meta::type_view`
 * 统一表达。外部查询优先使用 `reflection::type` 只读视图。
 */
struct type_data {
  vector<field_data> fields{};

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] std::optional<field_data> find_field(string_name name) const;
  [[nodiscard]] std::optional<field_data> find_field(std::string_view name) const;
};

/** @brief 创建类型反射底层数据。 */
[[nodiscard]] inline type_data make_type_data(std::initializer_list<field_data> fields) {
  return type_data{vector<field_data>{fields}};
}

} // namespace caors::reflection::detail

#endif
