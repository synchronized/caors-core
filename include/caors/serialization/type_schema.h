// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_TYPE_SCHEMA_H_
#define CAORS_SERIALIZATION_TYPE_SCHEMA_H_

#include "caors/core/containers.h"
#include "caors/core/string_name.h"
#include "caors/meta/capability_traits.h"
#include "caors/meta/type.h"

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <string_view>
#include <utility>

namespace caors::serialization {

class schema_view;

namespace detail {

struct type_runtime_schema;
struct type_schema_access;

} // namespace detail

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

/** @brief 用户注册时填写的单个字段序列化配置。 */
struct field_options {
  string_name field_name{};
  string_name serialized_name{};
  string_name legacy_serialized_name{};
  std::uint32_t field_id = 0;
  std::uint32_t legacy_field_id = 0;
  std::uint32_t version_added = 0;
  std::uint32_t version_removed = 0;
  bool required = false;
  bool skipped = false;

  [[nodiscard]] constexpr bool valid() const noexcept { return !field_name.empty(); }
};

/**
 * @brief 提交后的字段序列化 schema。
 *
 * `field_schema` 只保存公开字段配置。运行期属性视图、codec 和标量 ops 等调度缓存由序列化
 * 内部按需编译，不暴露在公开 schema 结构中。
 */
struct field_schema : field_options {
  field_schema() = default;
  explicit field_schema(field_options options) noexcept : field_options(std::move(options)) {}
};

/**
 * @brief 序列化类型元信息。
 *
 * 字段配置按字段名引用 meta 属性。注册提交时会把显式字段解析为 `meta::property`；未显式指定字段时，
 * 会按当前注册类型的属性列表生成默认字段。
 */
struct type_schema {
  vector<field_schema> fields{};
  bool has_explicit_fields = false;

  [[nodiscard]] bool valid() const noexcept {
    for (const field_schema& field : fields) {
      if (!field.valid()) {
        return false;
      }
    }

    return true;
  }

private:
  friend struct detail::type_schema_access;

  std::shared_ptr<const detail::type_runtime_schema> runtime_{};
};

/**
 * @brief 序列化 schema 的类型级 capability。
 *
 * capability 槽位按 `schema_capability` 查询，实际动态数据为每个注册类型独立的
 * `type_schema`。
 */
struct schema_capability {
  using data_type = type_schema;
  using view_type = schema_view;
};

/** @brief 序列化 schema 的类型级只读能力视图。 */
class schema_view {
public:
  using capability_type = schema_capability;
  using data_type = type_schema;

  schema_view() noexcept = default;
  explicit schema_view(const type_schema* schema) noexcept : schema_(schema) {}
  schema_view(meta::type owner, const type_schema* schema) noexcept
      : owner_(std::move(owner)), schema_(schema), owner_bound_(true) {}

  [[nodiscard]] bool valid() const noexcept {
    return schema_ != nullptr && (!owner_bound_ || owner_) && schema_->valid();
  }

  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

  [[nodiscard]] const type_schema* get() const noexcept { return valid() ? schema_ : nullptr; }
  [[nodiscard]] const type_schema* raw() const noexcept { return get(); }
  [[nodiscard]] std::size_t field_count() const noexcept {
    return valid() ? schema_->fields.size() : 0;
  }
  [[nodiscard]] const field_schema* field_at(std::size_t index) const noexcept {
    return valid() && index < schema_->fields.size() ? &schema_->fields[index] : nullptr;
  }
  [[nodiscard]] bool has_explicit_fields() const noexcept {
    return valid() && schema_->has_explicit_fields;
  }

private:
  meta::type owner_{};
  const type_schema* schema_ = nullptr;
  bool owner_bound_ = false;
};

/** @brief 创建字段序列化配置。 */
[[nodiscard]] inline field_options make_field(std::string_view field_name,
                                              std::string_view serialized_name = {},
                                              bool skipped = false, std::uint32_t field_id = 0) {
  return field_options{
      string_name{field_name},
      serialized_name.empty() ? string_name{field_name} : string_name{serialized_name},
      {},
      field_id,
      0,
      0,
      0,
      false,
      skipped,
  };
}

/** @brief 创建类型序列化配置。 */
[[nodiscard]] inline type_schema make_type_schema(std::initializer_list<field_options> fields) {
  type_schema schema{};
  schema.fields.reserve(fields.size());
  for (const field_options& field : fields) {
    schema.fields.emplace_back(field);
  }
  schema.has_explicit_fields = !schema.fields.empty();
  return schema;
}

} // namespace caors::serialization

#endif
