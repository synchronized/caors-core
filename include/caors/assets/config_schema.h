// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_ASSETS_CONFIG_SCHEMA_H_
#define CAORS_ASSETS_CONFIG_SCHEMA_H_

#include "caors/core/containers.h"
#include "caors/core/string_name.h"
#include "caors/meta/value/variant.h"

#include <cstdint>
#include <initializer_list>
#include <limits>
#include <string>
#include <string_view>

namespace caors::assets {

/** @brief source/type factory option 字段期望的数据形态。 */
enum class factory_option_kind {
  any,
  boolean,
  signed_integer,
  unsigned_integer,
  floating,
  string,
  array,
  dictionary,
  object,
};

/** @brief source/type factory 支持的单个 option 字段描述。 */
struct factory_option_field {
  using value_list = core::vector<core::string_name>;

  core::string_name name{};
  factory_option_kind kind = factory_option_kind::any;
  bool required = false;
  bool non_empty = false;
  bool has_range = false;
  double range_min = 0.0;
  double range_max = 0.0;
  value_list allowed_values{};
};

/** @brief source/type factory 的轻量 option schema。 */
struct factory_option_schema {
  using field_list = core::vector<factory_option_field>;

  explicit factory_option_schema(memory_resource* resource = nullptr)
      : fields(typename field_list::allocator_type{resource}) {}

  /** @brief 增加必填 option 字段。 */
  factory_option_schema& require(core::string_name name,
                                 factory_option_kind kind = factory_option_kind::any) {
    factory_option_field& field = ensure_field(name);
    field.kind = kind;
    field.required = true;
    return *this;
  }

  /** @brief 增加可选 option 字段。 */
  factory_option_schema& optional(core::string_name name,
                                  factory_option_kind kind = factory_option_kind::any) {
    factory_option_field& field = ensure_field(name);
    field.kind = kind;
    field.required = false;
    return *this;
  }

  /** @brief 要求字段值非空。当前用于字符串、数组、字典和对象。 */
  factory_option_schema& non_empty(core::string_name name) {
    ensure_field(name).non_empty = true;
    return *this;
  }

  /** @brief 要求数值字段位于闭区间 `[min, max]`。 */
  factory_option_schema& range(core::string_name name, double min, double max) {
    factory_option_field& field = ensure_field(name);
    field.has_range = true;
    field.range_min = min;
    field.range_max = max;
    return *this;
  }

  /** @brief 要求字符串字段必须是允许值之一。 */
  factory_option_schema& one_of(core::string_name name,
                                std::initializer_list<core::string_name> values) {
    factory_option_field& field = ensure_field(name);
    field.allowed_values.clear();
    for (core::string_name value : values) {
      field.allowed_values.push_back(value);
    }
    return *this;
  }

  /** @brief 要求字符串字段必须是允许值之一。 */
  factory_option_schema& one_of(core::string_name name,
                                std::initializer_list<std::string_view> values) {
    factory_option_field& field = ensure_field(name);
    field.allowed_values.clear();
    for (std::string_view value : values) {
      field.allowed_values.push_back(core::string_name{value});
    }
    return *this;
  }

  /** @brief 是否允许 schema 未声明的 option 字段。 */
  bool allow_unknown = true;
  field_list fields;

private:
  [[nodiscard]] memory_resource* field_resource() const noexcept {
    return fields.get_allocator().resource();
  }

  [[nodiscard]] factory_option_field make_field(core::string_name name) const {
    factory_option_field field;
    field.name = name;
    field.allowed_values = factory_option_field::value_list{
        typename factory_option_field::value_list::allocator_type{field_resource()}};
    return field;
  }

  [[nodiscard]] factory_option_field* find_property(core::string_name name) noexcept {
    for (factory_option_field& field : fields) {
      if (field.name == name) {
        return &field;
      }
    }

    return nullptr;
  }

  [[nodiscard]] factory_option_field& ensure_field(core::string_name name) {
    if (factory_option_field* field = find_property(name)) {
      return *field;
    }

    fields.push_back(make_field(name));
    return fields.back();
  }
};

/**
 * @brief source/type factory 读取 `options` 的轻量视图。
 *
 * 该视图不拥有 `variant`，字符串读取返回的 `std::string_view`
 * 只在底层配置对象存活且未修改期间有效。它只提供常见标量读取；复杂结构体绑定和跨字段校验应由上层
 * schema 或自定义 factory 处理。
 */
class factory_options_view final {
public:
  explicit factory_options_view(const variant& options) noexcept : options_(&options) {}

  /** @brief 判断 options 是否是 object。 */
  [[nodiscard]] bool is_object() const noexcept {
    return options_ != nullptr && options_->is_object();
  }

  /** @brief 按字段名查询原始 variant；options 不是 object 或字段不存在时返回 nullptr。 */
  [[nodiscard]] const variant* find(core::string_name name) const noexcept {
    return is_object() ? options_->find_member_variant(name) : nullptr;
  }

  /** @brief 判断字段是否存在且非 null。 */
  [[nodiscard]] bool contains(core::string_name name) const noexcept {
    const variant* value = find(name);
    return value != nullptr && !value->is_null();
  }

  /** @brief 读取 bool 字段。 */
  [[nodiscard]] bool read(core::string_name name, bool& out) const noexcept {
    const variant* value = find(name);
    if (value == nullptr) {
      return false;
    }

    if (const bool* typed = value->try_as_bool()) {
      out = *typed;
      return true;
    }

    return false;
  }

  /** @brief 读取有符号整数；无符号值超出 `std::int64_t` 范围时读取失败。 */
  [[nodiscard]] bool read(core::string_name name, std::int64_t& out) const noexcept {
    const variant* value = find(name);
    if (value == nullptr) {
      return false;
    }

    if (const std::int64_t* typed = value->try_as_i64()) {
      out = *typed;
      return true;
    }

    if (const std::uint64_t* typed = value->try_as_u64();
        typed != nullptr &&
        *typed <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
      out = static_cast<std::int64_t>(*typed);
      return true;
    }

    return false;
  }

  /** @brief 读取无符号整数；负数读取失败。 */
  [[nodiscard]] bool read(core::string_name name, std::uint64_t& out) const noexcept {
    const variant* value = find(name);
    if (value == nullptr) {
      return false;
    }

    if (const std::uint64_t* typed = value->try_as_u64()) {
      out = *typed;
      return true;
    }

    if (const std::int64_t* typed = value->try_as_i64(); typed != nullptr && *typed >= 0) {
      out = static_cast<std::uint64_t>(*typed);
      return true;
    }

    return false;
  }

  /** @brief 读取浮点数；整数会转换为 double。 */
  [[nodiscard]] bool read(core::string_name name, double& out) const noexcept {
    const variant* value = find(name);
    if (value == nullptr) {
      return false;
    }

    if (const double* typed = value->try_as_f64()) {
      out = *typed;
      return true;
    }
    if (const std::int64_t* typed = value->try_as_i64()) {
      out = static_cast<double>(*typed);
      return true;
    }
    if (const std::uint64_t* typed = value->try_as_u64()) {
      out = static_cast<double>(*typed);
      return true;
    }

    return false;
  }

  /** @brief 读取字符串视图；视图不拥有底层字符。 */
  [[nodiscard]] bool read(core::string_name name, std::string_view& out) const noexcept {
    const variant* value = find(name);
    if (value == nullptr) {
      return false;
    }

    if (const core::string* typed = value->try_as_string()) {
      out = std::string_view{typed->data(), typed->size()};
      return true;
    }

    return false;
  }

  /** @brief 读取字符串并复制到 `core::string`。 */
  [[nodiscard]] bool read(core::string_name name, core::string& out) const {
    std::string_view value;
    if (!read(name, value)) {
      return false;
    }

    out.assign(value.begin(), value.end());
    return true;
  }

  /** @brief 读取字符串并复制到 `std::string`。 */
  [[nodiscard]] bool read(core::string_name name, std::string& out) const {
    std::string_view value;
    if (!read(name, value)) {
      return false;
    }

    out.assign(value.begin(), value.end());
    return true;
  }

  /** @brief 读取字符串并 intern 为 `string_name`。 */
  [[nodiscard]] bool read(core::string_name name, core::string_name& out) const {
    std::string_view value;
    if (!read(name, value)) {
      return false;
    }

    out = core::string_name{value};
    return true;
  }

private:
  const variant* options_ = nullptr;
};

} // namespace caors::assets

#endif
