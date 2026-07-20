// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_PROPERTY_SCHEMA_H_
#define CAORS_META_PROPERTY_SCHEMA_H_

#include "caors/meta/metadata.h"

#include <initializer_list>
#include <string_view>
#include <utility>

namespace caors::meta {

/**
 * @brief 属性通用 schema。
 *
 * 该结构只描述属性本身的通用约束，适合配置校验、编辑器和工具层复用；序列化格式相关策略应放在
 * serialization 模块自己的 options 中。
 */
struct property_schema {
  bool required = false;
  bool non_empty = false;
  bool has_range_min = false;
  bool has_range_max = false;
  bool has_default_value = false;
  metadata_value range_min{};
  metadata_value range_max{};
  metadata_value one_of{};
  metadata_value default_value{};

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
  [[nodiscard]] const metadata_value* default_value_ptr() const noexcept {
    return has_default_value ? &default_value : nullptr;
  }
};

} // namespace caors::meta

namespace caors::meta::registration {

/** @brief 属性 schema 写入项类型。 */
enum class property_schema_entry_kind {
  required,
  non_empty,
  range_min,
  range_max,
  range,
  one_of,
  default_value,
};

/** @brief 一条待写入的属性 schema。 */
struct property_schema_entry {
  property_schema_entry_kind kind = property_schema_entry_kind::required;
  bool enabled = false;
  metadata_value first{};
  metadata_value second{};
};

/** @brief 将属性 schema 写入项应用到属性 schema。 */
inline void apply_property_schema_entry(property_schema& schema, property_schema_entry entry) {
  switch (entry.kind) {
  case property_schema_entry_kind::required:
    schema.required = entry.enabled;
    break;
  case property_schema_entry_kind::non_empty:
    schema.non_empty = entry.enabled;
    break;
  case property_schema_entry_kind::range_min:
    schema.has_range_min = true;
    schema.range_min = std::move(entry.first);
    break;
  case property_schema_entry_kind::range_max:
    schema.has_range_max = true;
    schema.range_max = std::move(entry.first);
    break;
  case property_schema_entry_kind::range:
    schema.has_range_min = true;
    schema.has_range_max = true;
    schema.range_min = std::move(entry.first);
    schema.range_max = std::move(entry.second);
    break;
  case property_schema_entry_kind::one_of:
    schema.one_of = std::move(entry.first);
    break;
  case property_schema_entry_kind::default_value:
    schema.has_default_value = true;
    schema.default_value = std::move(entry.first);
    break;
  }
}

/** @brief 声明属性必须有有效值。 */
[[nodiscard]] inline property_schema_entry required(bool enabled = true) {
  return property_schema_entry{property_schema_entry_kind::required, enabled};
}

/** @brief 声明属性不能为空。 */
[[nodiscard]] inline property_schema_entry non_empty(bool enabled = true) {
  return property_schema_entry{property_schema_entry_kind::non_empty, enabled};
}

/** @brief 声明属性最小值。 */
template <typename Value> [[nodiscard]] property_schema_entry range_min(Value&& value) {
  property_schema_entry entry{property_schema_entry_kind::range_min};
  entry.first = make_metadata_value(std::forward<Value>(value));
  return entry;
}

/** @brief 声明属性最大值。 */
template <typename Value> [[nodiscard]] property_schema_entry range_max(Value&& value) {
  property_schema_entry entry{property_schema_entry_kind::range_max};
  entry.first = make_metadata_value(std::forward<Value>(value));
  return entry;
}

/** @brief 声明属性数值范围。 */
template <typename Min, typename Max>
[[nodiscard]] property_schema_entry range(Min&& min, Max&& max) {
  property_schema_entry entry{property_schema_entry_kind::range};
  entry.first = make_metadata_value(std::forward<Min>(min));
  entry.second = make_metadata_value(std::forward<Max>(max));
  return entry;
}

/** @brief 声明属性可选字符串集合。 */
[[nodiscard]] inline property_schema_entry one_of(std::initializer_list<std::string_view> values) {
  property_schema_entry entry{property_schema_entry_kind::one_of};
  entry.first = make_metadata_string_array(values);
  return entry;
}

/** @brief 声明属性可选值集合。 */
[[nodiscard]] inline property_schema_entry one_of(metadata_value values) {
  property_schema_entry entry{property_schema_entry_kind::one_of};
  entry.first = std::move(values);
  return entry;
}

/** @brief 声明属性默认值。 */
template <typename Value> [[nodiscard]] property_schema_entry default_value(Value&& value) {
  property_schema_entry entry{property_schema_entry_kind::default_value};
  entry.first = make_metadata_value(std::forward<Value>(value));
  return entry;
}

} // namespace caors::meta::registration

#endif
