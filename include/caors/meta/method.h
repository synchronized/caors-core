// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_METHOD_H_
#define CAORS_META_METHOD_H_

#include "caors/export.h"
#include "caors/meta/argument.h"
#include "caors/meta/metadata.h"
#include "caors/meta/type.h"
#include "caors/meta/value/value.h"

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>

namespace caors::meta {

namespace detail {
class method_record;
class method_set_record;
class method_access;
} // namespace detail

class method_set;
class instance;

/** @brief 反射函数调用结果类型。 */
enum class invoke_result_kind {
  failed,
  void_value,
  value,
  reference,
};

/** @brief 高层运行期值调用结果类型。 */
enum class invoke_value_result_kind {
  failed,
  void_value,
  value,
};

/** @brief 方法匹配结果分类。 */
enum class method_match_kind {
  none,
  exact,
  conversion,
  ambiguous,
};

/** @brief 方法调用失败原因。 */
enum class invoke_failure_reason {
  none,
  invalid_method,
  invalid_object,
  const_violation,
  argument_mismatch,
  ambiguous_overload,
  invocation_failed,
};

/**
 * @brief 反射函数调用结果。
 *
 * `invoke_result` 明确区分调用失败、`void` 成功、拥有值返回和引用返回。
 */
class CAORS_API invoke_result final {
public:
  invoke_result() noexcept = default;
  invoke_result(const invoke_result& other)
      : kind_(other.kind_), failure_reason_(other.failure_reason_), value_(other.value_),
        ref_(other.ref_) {
    if (kind_ == invoke_result_kind::value && !value_) {
      kind_ = invoke_result_kind::failed;
      failure_reason_ = invoke_failure_reason::invocation_failed;
    }
  }
  invoke_result(invoke_result&&) noexcept = default;

  invoke_result& operator=(const invoke_result& other) {
    if (this == &other) {
      return *this;
    }

    kind_ = other.kind_;
    failure_reason_ = other.failure_reason_;
    value_ = other.value_;
    ref_ = other.ref_;
    if (kind_ == invoke_result_kind::value && !value_) {
      kind_ = invoke_result_kind::failed;
      failure_reason_ = invoke_failure_reason::invocation_failed;
    }
    return *this;
  }

  invoke_result& operator=(invoke_result&&) noexcept = default;

  [[nodiscard]] static invoke_result failed() noexcept { return {}; }
  [[nodiscard]] static invoke_result failed(invoke_failure_reason reason) noexcept {
    invoke_result result{};
    result.failure_reason_ = reason;
    return result;
  }
  [[nodiscard]] static invoke_result void_value() noexcept {
    invoke_result result{};
    result.kind_ = invoke_result_kind::void_value;
    return result;
  }
  [[nodiscard]] static invoke_result value(any value);
  [[nodiscard]] static invoke_result reference(any_ref ref) noexcept;

  [[nodiscard]] bool succeeded() const noexcept { return kind_ != invoke_result_kind::failed; }
  [[nodiscard]] explicit operator bool() const noexcept { return succeeded(); }
  [[nodiscard]] invoke_result_kind kind() const noexcept { return kind_; }
  [[nodiscard]] invoke_failure_reason failure_reason() const noexcept { return failure_reason_; }
  [[nodiscard]] bool has_value() const noexcept { return kind_ == invoke_result_kind::value; }
  [[nodiscard]] bool has_reference() const noexcept {
    return kind_ == invoke_result_kind::reference;
  }

  [[nodiscard]] const any& value() const noexcept { return value_; }
  [[nodiscard]] any& mutable_value() noexcept { return value_; }
  [[nodiscard]] any_ref ref() const noexcept { return ref_; }

  template <typename T>
  [[nodiscard]] const std::remove_cv_t<std::remove_reference_t<T>>* try_get() const noexcept {
    if (has_value()) {
      return value_.template try_get<T>();
    }

    return has_reference() ? ref_.template try_get<T>() : nullptr;
  }

  template <typename T>
  [[nodiscard]] std::remove_cv_t<std::remove_reference_t<T>>* try_get_mutable() noexcept {
    if (has_value()) {
      return value_.template try_get_mutable<T>();
    }

    return has_reference() ? ref_.template try_get_mutable<T>() : nullptr;
  }

private:
  invoke_result_kind kind_ = invoke_result_kind::failed;
  invoke_failure_reason failure_reason_ = invoke_failure_reason::none;
  any value_{};
  any_ref ref_{};
};

/**
 * @brief 使用 `meta::value` 表达返回值的反射函数调用结果。
 *
 * `invoke_value_result` 面向运行期高层 API。它保留失败原因和 `void` 成功语义，返回值统一包装为
 * `meta::value`；引用返回会保存为 `value::from_ref()`。
 */
class invoke_value_result final {
public:
  invoke_value_result() noexcept = default;

  [[nodiscard]] static invoke_value_result failed() noexcept { return {}; }
  [[nodiscard]] static invoke_value_result failed(invoke_failure_reason reason) noexcept {
    invoke_value_result result{};
    result.failure_reason_ = reason;
    return result;
  }
  [[nodiscard]] static invoke_value_result void_value() noexcept {
    invoke_value_result result{};
    result.kind_ = invoke_value_result_kind::void_value;
    return result;
  }
  [[nodiscard]] static invoke_value_result from_value(meta::value value) noexcept {
    invoke_value_result result{};
    result.kind_ = invoke_value_result_kind::value;
    result.value_ = std::move(value);
    return result;
  }

  [[nodiscard]] bool succeeded() const noexcept {
    return kind_ != invoke_value_result_kind::failed;
  }
  [[nodiscard]] explicit operator bool() const noexcept { return succeeded(); }
  [[nodiscard]] invoke_value_result_kind kind() const noexcept { return kind_; }
  [[nodiscard]] invoke_failure_reason failure_reason() const noexcept { return failure_reason_; }
  [[nodiscard]] bool has_value() const noexcept { return kind_ == invoke_value_result_kind::value; }

  [[nodiscard]] const meta::value& value() const noexcept { return value_; }
  [[nodiscard]] meta::value& mutable_value() noexcept { return value_; }

private:
  invoke_value_result_kind kind_ = invoke_value_result_kind::failed;
  invoke_failure_reason failure_reason_ = invoke_failure_reason::none;
  meta::value value_{};
};

/**
 * @brief 成员方法反射只读视图。
 *
 * `meta::method` 不拥有方法数据，只引用全局 `meta::registry`
 * 中的内部记录。调用时需要显式传入实例和参数；参数类型不匹配、对象为空或 const 对象调用非
 * const 方法时会返回失败结果。
 */
class CAORS_API method final {
public:
  method() noexcept = default;

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

  [[nodiscard]] string_name name() const noexcept;
  [[nodiscard]] meta::type get_owner_type() const noexcept;
  [[nodiscard]] meta::type get_return_type() const noexcept;
  [[nodiscard]] std::size_t parameter_count() const noexcept;
  [[nodiscard]] std::optional<meta::type> get_parameter_type(std::size_t index) const noexcept;
  [[nodiscard]] string_name get_parameter_name(std::size_t index) const noexcept;
  [[nodiscard]] bool const_method() const noexcept;
  [[nodiscard]] const meta::metadata_map& metadata() const noexcept;
  [[nodiscard]] const meta::metadata_value* find_metadata(meta::metadata_key key) const noexcept;
  [[nodiscard]] bool matches_arguments(std::span<const argument> arguments,
                                       argument_match_options options = {}) const noexcept;
  [[nodiscard]] bool matches_arguments(std::span<const named_argument> arguments,
                                       argument_match_options options = {}) const noexcept;
  [[nodiscard]] std::size_t default_argument_count() const noexcept;

  /** @brief 在实例上调用成员方法，保留 void / 值 / 引用返回语义。 */
  [[nodiscard]] invoke_result invoke_result_of(const instance& object,
                                               std::span<const argument> arguments = {},
                                               argument_match_options options = {}) const;
  [[nodiscard]] invoke_result invoke_result_of(const instance& object,
                                               std::span<const named_argument> arguments,
                                               argument_match_options options = {}) const;

  /** @brief 调用全局函数，保留 void / 值 / 引用返回语义。 */
  [[nodiscard]] invoke_result invoke_result_of(std::span<const argument> arguments = {},
                                               argument_match_options options = {}) const;
  [[nodiscard]] invoke_result invoke_result_of(std::span<const named_argument> arguments,
                                               argument_match_options options = {}) const;

  /** @brief 在实例上使用高层运行期值参数调用成员方法。 */
  [[nodiscard]] invoke_value_result invoke(const instance& object,
                                           std::span<const meta::value> arguments = {},
                                           argument_match_options options = {}) const;

  /** @brief 使用高层运行期值参数调用全局函数。 */
  [[nodiscard]] invoke_value_result invoke(std::span<const meta::value> arguments = {},
                                           argument_match_options options = {}) const;

  /** @brief 按指定类型调用方法并读取返回值。 */
  template <typename Return>
  [[nodiscard]] std::optional<std::remove_cv_t<std::remove_reference_t<Return>>>
  try_invoke(const instance& object, std::span<const argument> arguments = {}) const {
    using return_type = std::remove_cv_t<std::remove_reference_t<Return>>;
    static_assert(std::is_copy_constructible_v<return_type>,
                  "Reflected method return values must be copy constructible");

    invoke_result result = invoke_result_of(object, arguments);
    const return_type* value = result.template try_get<return_type>();
    if (value == nullptr) {
      return std::nullopt;
    }

    return *value;
  }

  /** @brief 按指定类型调用全局函数并读取返回值。 */
  template <typename Return>
  [[nodiscard]] std::optional<std::remove_cv_t<std::remove_reference_t<Return>>>
  try_invoke(std::span<const argument> arguments = {}) const {
    using return_type = std::remove_cv_t<std::remove_reference_t<Return>>;
    static_assert(std::is_copy_constructible_v<return_type>,
                  "Reflected method return values must be copy constructible");

    invoke_result result = invoke_result_of(arguments);
    const return_type* value = result.template try_get<return_type>();
    if (value == nullptr) {
      return std::nullopt;
    }

    return *value;
  }

private:
  friend class detail::method_access;

  method(const detail::method_record* reflected_method,
         std::shared_ptr<detail::registry_layer_state> layer) noexcept;
  [[nodiscard]] const detail::method_record* info() const noexcept;

  const detail::method_record* method_ = nullptr;
  std::shared_ptr<detail::registry_layer_state> layer_{};
};

/** @brief 同名方法重载集合的结构化匹配结果。 */
class method_match_result final {
public:
  method_match_result() noexcept = default;

  [[nodiscard]] static method_match_result none() noexcept { return {}; }
  [[nodiscard]] static method_match_result ambiguous() noexcept {
    method_match_result result{};
    result.kind_ = method_match_kind::ambiguous;
    return result;
  }
  [[nodiscard]] static method_match_result matched(method value, method_match_kind kind) noexcept {
    method_match_result result{};
    result.kind_ = kind;
    result.method_ = value;
    return result;
  }

  [[nodiscard]] method_match_kind kind() const noexcept { return kind_; }
  [[nodiscard]] bool matched() const noexcept {
    return kind_ == method_match_kind::exact || kind_ == method_match_kind::conversion;
  }
  [[nodiscard]] bool is_ambiguous() const noexcept { return kind_ == method_match_kind::ambiguous; }
  [[nodiscard]] explicit operator bool() const noexcept { return matched(); }
  [[nodiscard]] method value() const noexcept { return method_; }

private:
  method_match_kind kind_ = method_match_kind::none;
  method method_{};
};

/**
 * @brief 同名成员方法重载集合只读视图。
 *
 * `meta::method_set` 不拥有方法数据。它表示同一个方法名下的一组具体重载；调用时会根据
 * `argument` 参数匹配唯一重载，匹配失败或存在多个匹配时返回失败结果。
 */
class CAORS_API method_set final {
public:
  method_set() noexcept = default;

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

  [[nodiscard]] string_name name() const noexcept;
  [[nodiscard]] std::size_t overload_count() const noexcept;
  [[nodiscard]] method overload_at(std::size_t index) const;
  [[nodiscard]] method single() const;
  [[nodiscard]] method_match_result match(std::span<const argument> arguments = {},
                                          argument_match_options options = {}) const;
  [[nodiscard]] method_match_result match(std::span<const named_argument> arguments,
                                          argument_match_options options = {}) const;
  [[nodiscard]] method resolve(std::span<const argument> arguments = {},
                               argument_match_options options = {}) const;
  [[nodiscard]] method resolve(std::span<const named_argument> arguments,
                               argument_match_options options = {}) const;

  /** @brief 在实例上调用唯一匹配的成员方法重载，保留 void / 值 / 引用返回语义。 */
  [[nodiscard]] invoke_result invoke_result_of(const instance& object,
                                               std::span<const argument> arguments = {},
                                               argument_match_options options = {}) const;
  [[nodiscard]] invoke_result invoke_result_of(const instance& object,
                                               std::span<const named_argument> arguments,
                                               argument_match_options options = {}) const;

  /** @brief 调用唯一匹配的全局函数重载，保留 void / 值 / 引用返回语义。 */
  [[nodiscard]] invoke_result invoke_result_of(std::span<const argument> arguments = {},
                                               argument_match_options options = {}) const;
  [[nodiscard]] invoke_result invoke_result_of(std::span<const named_argument> arguments,
                                               argument_match_options options = {}) const;

  /** @brief 在实例上使用高层运行期值参数调用唯一匹配的成员方法重载。 */
  [[nodiscard]] invoke_value_result invoke(const instance& object,
                                           std::span<const meta::value> arguments = {},
                                           argument_match_options options = {}) const;

  /** @brief 使用高层运行期值参数调用唯一匹配的全局函数重载。 */
  [[nodiscard]] invoke_value_result invoke(std::span<const meta::value> arguments = {},
                                           argument_match_options options = {}) const;

private:
  friend class detail::method_access;

  method_set(const detail::method_set_record* reflected_method_set,
             std::shared_ptr<detail::registry_layer_state> layer) noexcept;
  [[nodiscard]] const detail::method_set_record* info() const noexcept;

  const detail::method_set_record* method_set_ = nullptr;
  std::shared_ptr<detail::registry_layer_state> layer_{};
};

} // namespace caors::meta

#endif
