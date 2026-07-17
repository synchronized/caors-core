// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_FIELD_SCHEMA_H_
#define CAORS_META_FIELD_SCHEMA_H_

#include "caors/meta/metadata.h"

#include <initializer_list>
#include <string_view>
#include <utility>

namespace caors::meta {

/**
 * @brief 字段通用 schema。
 *
 * 该结构只描述字段本身的通用约束，适合配置校验、编辑器和工具层复用；序列化格式相关策略应放在
 * serialization 模块自己的 options 中。
 */
struct field_schema {
  bool required = false;
  bool non_empty = false;
  bool has_range_min = false;
  bool has_range_max = false;
  metadata_value range_min{};
  metadata_value range_max{};
  metadata_value one_of{};

  [[nodiscard]] bool has_range() const noexcept { return has_range_min || has_range_max; }
  [[nodiscard]] bool has_one_of() const noexcept {
    const metadata_value::array_type* values = one_of.try_as_array();
    return values != nullptr && !values->empty();
  }
  [[nodiscard]] const metadata_value* range_min_value() const noexcept {
    return has_range_min ? &range_min : nullptr;
  }
  [[nodiscard]] const metadata_value* range_max_value() const noexcept {
    return has_range_max ? &range_max : nullptr;
  }
  [[nodiscard]] const metadata_value* one_of_value() const noexcept {
    return has_one_of() ? &one_of : nullptr;
  }
};

} // namespace caors::meta

namespace caors::meta::registration {

/** @brief 字段 schema 写入项类型。 */
enum class field_schema_entry_kind {
  required,
  non_empty,
  range_min,
  range_max,
  range,
  one_of,
};

/** @brief 一条待写入的字段 schema。 */
struct field_schema_entry {
  field_schema_entry_kind kind = field_schema_entry_kind::required;
  bool enabled = false;
  metadata_value first{};
  metadata_value second{};
};

/** @brief 将字段 schema 写入项应用到字段 schema。 */
inline void apply_field_schema_entry(field_schema& schema, field_schema_entry entry) {
  switch (entry.kind) {
  case field_schema_entry_kind::required:
    schema.required = entry.enabled;
    break;
  case field_schema_entry_kind::non_empty:
    schema.non_empty = entry.enabled;
    break;
  case field_schema_entry_kind::range_min:
    schema.has_range_min = true;
    schema.range_min = std::move(entry.first);
    break;
  case field_schema_entry_kind::range_max:
    schema.has_range_max = true;
    schema.range_max = std::move(entry.first);
    break;
  case field_schema_entry_kind::range:
    schema.has_range_min = true;
    schema.has_range_max = true;
    schema.range_min = std::move(entry.first);
    schema.range_max = std::move(entry.second);
    break;
  case field_schema_entry_kind::one_of:
    schema.one_of = std::move(entry.first);
    break;
  }
}

/** @brief 声明字段必须有有效值。 */
[[nodiscard]] inline field_schema_entry required(bool enabled = true) {
  return field_schema_entry{field_schema_entry_kind::required, enabled};
}

/** @brief 声明字段不能为空。 */
[[nodiscard]] inline field_schema_entry non_empty(bool enabled = true) {
  return field_schema_entry{field_schema_entry_kind::non_empty, enabled};
}

/** @brief 声明字段最小值。 */
template <typename Value> [[nodiscard]] field_schema_entry range_min(Value&& value) {
  field_schema_entry entry{field_schema_entry_kind::range_min};
  entry.first = make_metadata_value(std::forward<Value>(value));
  return entry;
}

/** @brief 声明字段最大值。 */
template <typename Value> [[nodiscard]] field_schema_entry range_max(Value&& value) {
  field_schema_entry entry{field_schema_entry_kind::range_max};
  entry.first = make_metadata_value(std::forward<Value>(value));
  return entry;
}

/** @brief 声明字段数值范围。 */
template <typename Min, typename Max> [[nodiscard]] field_schema_entry range(Min&& min, Max&& max) {
  field_schema_entry entry{field_schema_entry_kind::range};
  entry.first = make_metadata_value(std::forward<Min>(min));
  entry.second = make_metadata_value(std::forward<Max>(max));
  return entry;
}

/** @brief 声明字段可选字符串集合。 */
[[nodiscard]] inline field_schema_entry one_of(std::initializer_list<std::string_view> values) {
  field_schema_entry entry{field_schema_entry_kind::one_of};
  entry.first = make_metadata_string_array(values);
  return entry;
}

/** @brief 声明字段可选值集合。 */
[[nodiscard]] inline field_schema_entry one_of(metadata_value values) {
  field_schema_entry entry{field_schema_entry_kind::one_of};
  entry.first = std::move(values);
  return entry;
}

} // namespace caors::meta::registration

#endif
