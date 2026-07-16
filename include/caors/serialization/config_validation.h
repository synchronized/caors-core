// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_CONFIG_VALIDATION_H_
#define CAORS_SERIALIZATION_CONFIG_VALIDATION_H_

#include "caors/core/containers.h"
#include "caors/meta/metadata.h"
#include "caors/reflection/registry.h"

#include <optional>
#include <string>
#include <string_view>

namespace caors::serialization {

/** @brief 配置字段 metadata 约定键。 */
namespace config_metadata {

[[nodiscard]] inline meta::metadata_key required() { return meta::metadata_key{"required"}; }
[[nodiscard]] inline meta::metadata_key non_empty() { return meta::metadata_key{"non_empty"}; }
[[nodiscard]] inline meta::metadata_key range_min() { return meta::metadata_key{"range_min"}; }
[[nodiscard]] inline meta::metadata_key range_max() { return meta::metadata_key{"range_max"}; }
[[nodiscard]] inline meta::metadata_key one_of() { return meta::metadata_key{"one_of"}; }

} // namespace config_metadata

/** @brief 单条配置校验诊断。 */
struct config_validation_diagnostic {
  core::string_name field{};
  core::string reason{};
};

/** @brief 配置校验结果。 */
struct config_validation_result {
  using diagnostic_list = core::vector<config_validation_diagnostic>;

  explicit config_validation_result(memory_resource* resource = nullptr)
      : diagnostics(typename diagnostic_list::allocator_type{resource}) {}

  error status{};
  diagnostic_list diagnostics;

  [[nodiscard]] bool succeeded() const noexcept { return status.succeeded(); }
  [[nodiscard]] bool failed() const noexcept { return status.failed(); }
  [[nodiscard]] explicit operator bool() const noexcept { return succeeded(); }
};

namespace detail {

[[nodiscard]] inline core::string make_config_validation_reason(memory_resource* resource) {
  return core::string{allocator<char>{resource}};
}

inline void append_field_name(core::string& reason, core::string_name name) {
  const std::string_view view = name.view();
  reason.append(view.begin(), view.end());
}

inline void append_reason(config_validation_result& result, core::string_name field,
                          std::string_view text) {
  memory_resource* resource = result.diagnostics.get_allocator().resource();
  core::string reason = make_config_validation_reason(resource);
  reason.assign(text.begin(), text.end());
  result.diagnostics.push_back({field, std::move(reason)});
  result.status = {error_domain::serialization, errc::invalid_argument};
}

[[nodiscard]] inline bool metadata_truthy(const meta::metadata_value* value) noexcept {
  return value != nullptr && value->is_bool() && value->as_bool();
}

[[nodiscard]] inline std::optional<double>
metadata_number(const meta::metadata_value* value) noexcept {
  if (value == nullptr) {
    return std::nullopt;
  }

  if (const std::int64_t* number = value->try_as_i64()) {
    return static_cast<double>(*number);
  }
  if (const std::uint64_t* number = value->try_as_u64()) {
    return static_cast<double>(*number);
  }
  if (const double* number = value->try_as_f64()) {
    return *number;
  }

  return std::nullopt;
}

[[nodiscard]] inline std::optional<double> ref_number(any_ref value) noexcept {
  if (const auto* number = value.try_get<signed char>()) {
    return static_cast<double>(*number);
  }
  if (const auto* number = value.try_get<short>()) {
    return static_cast<double>(*number);
  }
  if (const auto* number = value.try_get<int>()) {
    return static_cast<double>(*number);
  }
  if (const auto* number = value.try_get<long>()) {
    return static_cast<double>(*number);
  }
  if (const auto* number = value.try_get<long long>()) {
    return static_cast<double>(*number);
  }
  if (const auto* number = value.try_get<unsigned char>()) {
    return static_cast<double>(*number);
  }
  if (const auto* number = value.try_get<unsigned short>()) {
    return static_cast<double>(*number);
  }
  if (const auto* number = value.try_get<unsigned int>()) {
    return static_cast<double>(*number);
  }
  if (const auto* number = value.try_get<unsigned long>()) {
    return static_cast<double>(*number);
  }
  if (const auto* number = value.try_get<unsigned long long>()) {
    return static_cast<double>(*number);
  }
  if (const auto* number = value.try_get<float>()) {
    return static_cast<double>(*number);
  }
  if (const auto* number = value.try_get<double>()) {
    return *number;
  }

  return std::nullopt;
}

[[nodiscard]] inline std::optional<std::string_view> ref_text(any_ref value) noexcept {
  if (const auto* text = value.try_get<core::string>()) {
    return std::string_view{text->data(), text->size()};
  }
  if (const auto* text = value.try_get<std::string>()) {
    return std::string_view{text->data(), text->size()};
  }
  if (const auto* text = value.try_get<std::string_view>()) {
    return *text;
  }
  if (const auto* text = value.try_get<core::string_name>()) {
    return text->view();
  }

  return std::nullopt;
}

[[nodiscard]] inline bool ref_empty(any_ref value) noexcept {
  if (!value) {
    return true;
  }

  if (const std::optional<std::string_view> text = ref_text(value); text.has_value()) {
    return text->empty();
  }

  return false;
}

[[nodiscard]] inline bool allowed_by_one_of(any_ref value,
                                            const meta::metadata_value* one_of) noexcept {
  if (one_of == nullptr) {
    return true;
  }

  const meta::metadata_value::array_type* allowed = one_of->try_as_array();
  if (allowed == nullptr || allowed->empty()) {
    return true;
  }

  const std::optional<std::string_view> text = ref_text(value);
  if (!text.has_value()) {
    return false;
  }

  for (const meta::metadata_value& item : *allowed) {
    const core::string* allowed_text = item.try_as_string();
    if (allowed_text != nullptr &&
        *text == std::string_view{allowed_text->data(), allowed_text->size()}) {
      return true;
    }
  }

  return false;
}

inline void validate_config_field(config_validation_result& result, const reflection::field& field,
                                  const void* object) {
  const any_ref value = field.get_ref(object);
  if (!value) {
    core::string reason =
        make_config_validation_reason(result.diagnostics.get_allocator().resource());
    reason.assign("配置字段无法读取：");
    append_field_name(reason, field.name());
    result.diagnostics.push_back({field.name(), std::move(reason)});
    result.status = {error_domain::serialization, errc::invalid_argument};
    return;
  }

  const bool required = metadata_truthy(field.find_metadata(config_metadata::required()));
  const bool non_empty = metadata_truthy(field.find_metadata(config_metadata::non_empty()));
  if ((required || non_empty) && ref_empty(value)) {
    core::string reason =
        make_config_validation_reason(result.diagnostics.get_allocator().resource());
    reason.assign(required ? "配置字段缺少有效值：" : "配置字段不能为空：");
    append_field_name(reason, field.name());
    result.diagnostics.push_back({field.name(), std::move(reason)});
    result.status = {error_domain::serialization, errc::invalid_argument};
  }

  const std::optional<double> min =
      metadata_number(field.find_metadata(config_metadata::range_min()));
  const std::optional<double> max =
      metadata_number(field.find_metadata(config_metadata::range_max()));
  if (min.has_value() || max.has_value()) {
    const std::optional<double> number = ref_number(value);
    if (!number.has_value() || (min.has_value() && *number < *min) ||
        (max.has_value() && *number > *max)) {
      core::string reason =
          make_config_validation_reason(result.diagnostics.get_allocator().resource());
      reason.assign("配置字段超出范围：");
      append_field_name(reason, field.name());
      result.diagnostics.push_back({field.name(), std::move(reason)});
      result.status = {error_domain::serialization, errc::invalid_argument};
    }
  }

  const meta::metadata_value* one_of = field.find_metadata(config_metadata::one_of());
  if (!allowed_by_one_of(value, one_of)) {
    core::string reason =
        make_config_validation_reason(result.diagnostics.get_allocator().resource());
    reason.assign("配置字段不是允许值：");
    append_field_name(reason, field.name());
    result.diagnostics.push_back({field.name(), std::move(reason)});
    result.status = {error_domain::serialization, errc::invalid_argument};
  }
}

} // namespace detail

/** @brief 使用反射字段 metadata 校验配置对象。 */
template <typename T>
[[nodiscard]] config_validation_result validate_config(const T& object,
                                                       memory_resource* resource = nullptr) {
  config_validation_result result{resource};
  const meta::type meta_type = meta::find<T>();
  if (!meta_type.registered()) {
    result.status = {error_domain::serialization, errc::missing_type};
    return result;
  }

  const reflection::type reflected = reflection::find(meta_type.id());
  if (!reflected) {
    result.status = {error_domain::serialization, errc::not_found};
    return result;
  }

  for (std::size_t index = 0; index < reflected.field_count(); ++index) {
    const reflection::field field = reflected.field_at(index);
    if (!field) {
      result.status = {error_domain::serialization, errc::invalid_field};
      return result;
    }

    detail::validate_config_field(result, field, &object);
  }

  return result;
}

} // namespace caors::serialization

#endif
