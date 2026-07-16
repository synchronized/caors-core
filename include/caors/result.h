// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_RESULT_H_
#define CAORS_RESULT_H_

#include "caors/error.h"

#include <memory>
#include <utility>

namespace caors {

/**
 * @brief 携带值的轻量返回结果。
 *
 * `result<T>` 保存一个错误状态和一个值。布尔上下文中 `true` 表示有可用值，`false` 表示失败。
 * 失败时 `status` 保存错误原因，`value` 保持默认构造状态。
 *
 * 新代码优先使用 `has_value()`、`error()`、`operator*`、`operator->` 和 `value_or()`，避免直接依赖
 * 数据成员名称。`status` / `value` 仍保留为轻量聚合字段，方便当前模块逐步迁移。
 */
template <typename T, typename E = error> struct result {
  using value_type = T;
  using error_type = E;

  E status{};
  T value{};

  [[nodiscard]] bool succeeded() const noexcept { return !static_cast<bool>(status); }
  [[nodiscard]] bool failed() const noexcept { return static_cast<bool>(status); }
  [[nodiscard]] bool has_value() const noexcept { return succeeded(); }
  [[nodiscard]] explicit operator bool() const noexcept { return succeeded(); }

  [[nodiscard]] E& error() noexcept { return status; }
  [[nodiscard]] const E& error() const noexcept { return status; }

  [[nodiscard]] T& operator*() & noexcept { return value; }
  [[nodiscard]] const T& operator*() const& noexcept { return value; }
  [[nodiscard]] T&& operator*() && noexcept { return std::move(value); }
  [[nodiscard]] const T&& operator*() const&& noexcept { return std::move(value); }

  [[nodiscard]] T* operator->() noexcept { return std::addressof(value); }
  [[nodiscard]] const T* operator->() const noexcept { return std::addressof(value); }

  template <typename U> [[nodiscard]] T value_or(U&& fallback) const& {
    return has_value() ? value : static_cast<T>(std::forward<U>(fallback));
  }

  template <typename U> [[nodiscard]] T value_or(U&& fallback) && {
    return has_value() ? std::move(value) : static_cast<T>(std::forward<U>(fallback));
  }
};

} // namespace caors

#endif
