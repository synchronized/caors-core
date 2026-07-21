// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_TYPE_SCHEMA_H_
#define CAORS_SERIALIZATION_TYPE_SCHEMA_H_

#include "caors/core/containers.h"
#include "caors/core/string_name.h"
#include "caors/meta/capability_traits.h"
#include "caors/meta/detail/type_extension_templates.h"
#include "caors/meta/property.h"

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string_view>
#include <utility>

namespace caors::meta::detail {
class property_data;
} // namespace caors::meta::detail

namespace caors::serialization {

struct codec_ops;

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
  std::uint32_t field_id = 0;
  std::uint32_t version_added = 0;
  std::uint32_t version_removed = 0;
  bool required = false;
  bool skipped = false;

  [[nodiscard]] constexpr bool valid() const noexcept { return !field_name.empty(); }
};

/**
 * @brief 提交后的运行期字段 schema。
 *
 * `field_schema` 保留用户字段配置，并额外保存提交时绑定好的 `meta::property` 视图。
 */
struct field_schema : field_options {
  meta::property property{};
  const meta::detail::property_data* property_data = nullptr;
  meta::type owner_type{};
  meta::type declared_type{};
  meta::type value_type{};
  const serialization::codec_ops* codec_operations = nullptr;
  const meta::scalar_ops* scalar_operations = nullptr;
  bool uses_direct_value = false;
  bool uses_inline_indirect_value = false;

  field_schema() = default;
  field_schema(field_options options, meta::property reflected_property = {}) noexcept
      : field_options(std::move(options)) {
    bind_property(reflected_property);
  }

  /** @brief 绑定提交期解析出的属性视图，并缓存运行期调度所需的字段语义。 */
  void bind_property(meta::property reflected_property) noexcept {
    property = reflected_property;
    property_data = property ? meta::detail::property_data_of(property) : nullptr;
    owner_type = property ? property.get_owner_type() : meta::type{};
    declared_type = property ? property.get_type() : meta::type{};
    value_type = property ? property.get_value_type() : meta::type{};
    codec_operations =
        declared_type ? declared_type.get_raw_ops<serialization::codec_ops>() : nullptr;
    scalar_operations = declared_type ? declared_type.get_raw_ops<meta::scalar_ops>() : nullptr;
    const meta::type_kind kind = property ? property.kind() : meta::type_kind::value;
    uses_inline_indirect_value =
        property && (kind == meta::type_kind::raw_pointer || kind == meta::type_kind::wrapper) &&
        property.wrapper_semantics() == meta::wrapper_semantics::inline_value;
    uses_direct_value =
        property && (kind == meta::type_kind::value || kind == meta::type_kind::enumeration ||
                     kind == meta::type_kind::sequence || kind == meta::type_kind::set ||
                     kind == meta::type_kind::associative || kind == meta::type_kind::pair ||
                     kind == meta::type_kind::tuple || kind == meta::type_kind::variant);
  }
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
};

/** @brief 序列化 schema 的类型级只读能力视图。 */
class schema_view {
public:
  schema_view() noexcept = default;
  explicit schema_view(const type_schema* schema) noexcept : schema_(schema) {}

  [[nodiscard]] bool valid() const noexcept { return schema_ != nullptr && schema_->valid(); }

  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

  [[nodiscard]] const type_schema* get() const noexcept { return valid() ? schema_ : nullptr; }
  [[nodiscard]] const type_schema* raw() const noexcept { return schema_; }
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
  const type_schema* schema_ = nullptr;
};

/** @brief 创建字段序列化配置。 */
[[nodiscard]] inline field_options make_field(std::string_view field_name,
                                              std::string_view serialized_name = {},
                                              bool skipped = false, std::uint32_t field_id = 0) {
  return field_options{
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

namespace caors::meta {

/** @brief `serialization::type_schema` 的类型级能力查询特化。 */
template <> struct type_capability_traits<serialization::schema_view> {
  using result_type = serialization::schema_view;

  [[nodiscard]] static result_type get(const type& source) noexcept {
    return result_type{detail::find_type_extension<serialization::type_schema>(source)};
  }

  [[nodiscard]] static const serialization::type_schema* get_raw(const type& source) noexcept {
    return detail::find_type_extension<serialization::type_schema>(source);
  }

  [[nodiscard]] static bool has(const type& source) noexcept { return get(source).valid(); }
};

} // namespace caors::meta

#endif
