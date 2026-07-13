// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_ERROR_H_
#define CAORS_ERROR_H_

#include "caors/export.h"

#include <system_error>
#include <type_traits>

namespace caors {

/** @brief 通用错误码，供跨模块 API 和 `std::error_code` 适配使用。 */
enum class errc {
  success = 0,
  invalid_argument,
  invalid_type,
  invalid_field,
  missing_type,
  not_found,
  already_exists,
  duplicate_id,
  duplicate_name,
  duplicate_guid,
  duplicate_component,
  duplicate_field,
  duplicate_method,
  duplicate_reflection,
  inconsistent_type,
  missing_parent,
  frozen,
  pending,
  cancelled,
  cycle_detected,
  recursion_limit,
  unsupported,
  out_of_memory,
  backend_failure,
};

/** @brief 错误来源域，用于在统一错误码之外保留模块边界。 */
enum class error_domain {
  none,
  core,
  io,
  meta,
  memory,
  object,
  reflection,
  resources,
  serialization,
};

/**
 * @brief 轻量错误状态。
 *
 * `error` 不拥有动态消息，也不分配内存。需要和标准库互操作时可以通过 `code()` 转为
 * `std::error_code`。布尔上下文中 `true` 表示失败，`false` 表示成功。
 */
class error {
public:
  constexpr error() noexcept = default;
  constexpr error(error_domain domain, errc code) noexcept : domain_(domain), code_(code) {}

  [[nodiscard]] static constexpr error ok() noexcept { return {}; }

  [[nodiscard]] constexpr error_domain domain() const noexcept { return domain_; }
  [[nodiscard]] constexpr errc value() const noexcept { return code_; }
  [[nodiscard]] constexpr bool succeeded() const noexcept { return code_ == errc::success; }
  [[nodiscard]] constexpr bool failed() const noexcept { return !succeeded(); }
  [[nodiscard]] explicit constexpr operator bool() const noexcept { return failed(); }
  [[nodiscard]] std::error_code code() const noexcept;

  friend constexpr bool operator==(error lhs, error rhs) noexcept {
    return lhs.domain_ == rhs.domain_ && lhs.code_ == rhs.code_;
  }

  friend constexpr bool operator!=(error lhs, error rhs) noexcept { return !(lhs == rhs); }

private:
  error_domain domain_ = error_domain::none;
  errc code_ = errc::success;
};

/** @brief 获取 CAORS 通用错误分类。 */
[[nodiscard]] CAORS_API const std::error_category& error_category() noexcept;

/** @brief 将 CAORS 通用错误码转换为 `std::error_code`。 */
[[nodiscard]] CAORS_API std::error_code make_error_code(errc code) noexcept;

inline std::error_code error::code() const noexcept { return make_error_code(code_); }

/** @brief 将错误状态作为另一个模块的错误重新返回，仅替换错误来源域。 */
[[nodiscard]] constexpr error rethrow_as(error status, error_domain domain) noexcept {
  if (!status) {
    return {};
  }

  return {domain, status.value()};
}

/**
 * @brief 将错误状态作为另一个模块的错误重新返回，并覆盖一个模块边界上的语义差异。
 *
 * 当底层错误码在当前模块有更准确的对外语义时使用。例如 meta 的
 * `duplicate_component` 可以在 reflection 边界转换为 `duplicate_reflection`。
 */
[[nodiscard]] constexpr error rethrow_as(error status, error_domain domain, errc from,
                                         errc to) noexcept {
  if (!status) {
    return {};
  }

  return {domain, status.value() == from ? to : status.value()};
}

} // namespace caors

namespace std {

template <> struct is_error_code_enum<caors::errc> : true_type {};

} // namespace std

#endif
