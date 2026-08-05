// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_ASSETS_CONFIG_SCHEMA_H_
#define CAORS_ASSETS_CONFIG_SCHEMA_H_

#include "caors/core/containers.h"
#include "caors/core/string_name.h"
#include "caors/meta/value/variant.h"
#include "caors/serialization/config_validation.h"
#include "caors/serialization/operations.h"

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace caors::assets {

/**
 * @brief source/type factory 读取 `options` 的轻量视图。
 *
 * 该视图不拥有 `variant`，字符串读取返回的 `std::string_view`
 * 只在底层配置对象存活且未修改期间有效。它只提供常见标量读取；复杂结构体绑定和跨字段校验应由上层
 * typed factory 或自定义 factory 处理。
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

/** @brief typed factory options 读入和校验结果。 */
template <typename Options> struct typed_factory_options_result {
  explicit typed_factory_options_result(memory_resource* result_resource = nullptr)
      : validation(result_resource) {}

  error status{};
  Options value{};
  serialization::config_validation_result validation;

  [[nodiscard]] bool succeeded() const noexcept { return !status; }
  [[nodiscard]] bool failed() const noexcept { return status.failed(); }
  [[nodiscard]] explicit operator bool() const noexcept { return succeeded(); }
};

namespace detail {

[[nodiscard]] inline core::string
make_typed_factory_option_reason(memory_resource* result_resource) {
  return core::string{allocator<char>{result_resource}};
}

inline void append_name(core::string& text, core::string_name name) {
  const std::string_view view = name.view();
  text.append(view.begin(), view.end());
}

inline void append_typed_factory_option_diagnostic(serialization::config_validation_result& result,
                                                   core::string_name field, std::string_view text) {
  core::string reason =
      make_typed_factory_option_reason(result.diagnostics.get_allocator().resource());
  reason.assign(text.begin(), text.end());
  result.diagnostics.push_back({field, std::move(reason)});
  result.status = {error_domain::serialization, errc::invalid_argument};
}

template <typename Options>
void append_unknown_option_diagnostics(typed_factory_options_result<Options>& result,
                                       const variant& options) {
  if (!options.is_object()) {
    return;
  }

  const meta::type type = meta::find<Options>();
  if (!type.registered()) {
    append_typed_factory_option_diagnostic(result.validation, {},
                                           "typed factory options 类型尚未注册 meta");
    result.status = {error_domain::serialization, errc::missing_type};
    return;
  }

  for (const meta::variant_member& member : options.as_object()) {
    if (!type.find_property(member.name)) {
      core::string reason = make_typed_factory_option_reason(
          result.validation.diagnostics.get_allocator().resource());
      reason.assign("options 字段未声明：");
      append_name(reason, member.name);
      result.validation.diagnostics.push_back({member.name, std::move(reason)});
      result.validation.status = {error_domain::serialization, errc::invalid_field};
      result.status = result.validation.status;
    }
  }
}

inline void append_typed_factory_status_diagnostic(serialization::config_validation_result& result,
                                                   error status) {
  if (!status || !result.diagnostics.empty()) {
    return;
  }

  switch (status.value()) {
  case errc::missing_type:
    append_typed_factory_option_diagnostic(result, {}, "typed factory options 类型尚未注册 meta");
    break;
  case errc::invalid_field:
    append_typed_factory_option_diagnostic(result, {}, "typed factory options 包含无效字段");
    break;
  case errc::invalid_argument:
    append_typed_factory_option_diagnostic(result, {}, "typed factory options 参数无效");
    break;
  case errc::unsupported:
    append_typed_factory_option_diagnostic(result, {}, "typed factory options 包含不支持的配置值");
    break;
  default:
    append_typed_factory_option_diagnostic(result, {}, "typed factory options 读取失败");
    break;
  }
}

inline void append_validation_diagnostics(serialization::config_validation_result& target,
                                          const serialization::config_validation_result& source) {
  for (const serialization::config_validation_diagnostic& diagnostic : source.diagnostics) {
    core::string reason =
        make_typed_factory_option_reason(target.diagnostics.get_allocator().resource());
    reason.assign(diagnostic.reason.begin(), diagnostic.reason.end());
    target.diagnostics.push_back({diagnostic.field, std::move(reason)});
  }

  if (source.status && !target.status) {
    target.status = source.status;
  }
}

} // namespace detail

/**
 * @brief 把 factory `options` DOM 读入强类型配置对象，并执行通用配置 schema 校验。
 *
 * 该 helper 面向复杂 source/type factory。它先应用 `meta::property_schema` 默认值，
 * 再用 `read_value_from()` 把 DOM 覆盖到结构体，最后调用 `validate_config()`。
 * 默认拒绝 schema 未声明字段，避免插件配置里出现拼写错误后被静默忽略。
 */
template <typename Options>
[[nodiscard]] typed_factory_options_result<Options>
read_typed_factory_options(const variant& options, memory_resource* result_resource = nullptr,
                           serialization::unknown_field_policy unknown_fields =
                               serialization::unknown_field_policy::reject) {
  static_assert(std::is_default_constructible_v<Options>,
                "Typed factory options must be default constructible");

  typed_factory_options_result<Options> result{result_resource};

  result.status = serialization::apply_config_defaults(result.value, result_resource);
  if (result.status) {
    detail::append_typed_factory_status_diagnostic(result.validation, result.status);
    return result;
  }

  if (unknown_fields == serialization::unknown_field_policy::reject) {
    detail::append_unknown_option_diagnostics(result, options);
  }

  if (!options.is_null()) {
    serialization::context context{};
    context.config.schema.unknown_fields =
        unknown_fields == serialization::unknown_field_policy::reject
            ? serialization::unknown_field_policy::ignore
            : unknown_fields;
    context.config.schema.missing_fields = serialization::missing_field_policy::keep_existing;
    result.status = serialization::read_value_from(context, result.value, options);
    if (result.status) {
      detail::append_typed_factory_status_diagnostic(result.validation, result.status);
      return result;
    }
  }

  serialization::config_validation_result validation =
      serialization::validate_config(result.value, result_resource);
  detail::append_validation_diagnostics(result.validation, validation);
  if (result.validation.failed() && !result.status) {
    result.status = result.validation.status;
  }
  return result;
}

/** @brief 把 typed factory options 诊断追加到资产构建原因列表。 */
template <typename Options>
void append_typed_factory_option_reasons(core::vector<core::string>& reasons,
                                         const typed_factory_options_result<Options>& result) {
  memory_resource* resource = reasons.get_allocator().resource();
  for (const serialization::config_validation_diagnostic& diagnostic :
       result.validation.diagnostics) {
    core::string reason = detail::make_typed_factory_option_reason(resource);
    reason.assign(diagnostic.reason.begin(), diagnostic.reason.end());
    reasons.push_back(std::move(reason));
  }

  if (result.failed() && result.validation.diagnostics.empty()) {
    core::string reason = detail::make_typed_factory_option_reason(resource);
    reason.assign("typed factory options 读取失败");
    reasons.push_back(std::move(reason));
  }
}

} // namespace caors::assets

#endif
