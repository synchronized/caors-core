// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_ARGUMENT_H_
#define CAORS_META_ARGUMENT_H_

#include "caors/meta/value/any_ref.h"

#include <type_traits>
#include <utility>

namespace caors::meta {

/**
 * @brief 反射方法调用参数。
 *
 * `argument` 用来表达方法调用参数的 value category。普通 `any_ref` 只能表示左值引用；
 * `argument` 在此基础上增加拥有值的右值参数，用于匹配和调用 `T&&` 参数。
 */
class argument final {
public:
  argument() noexcept = default;

  /** @brief 从已有运行期引用创建左值参数。 */
  [[nodiscard]] static argument from(any_ref value) noexcept {
    argument result{};
    result.ref_ = value;
    return result;
  }

  /** @brief 从可写 C++ 对象创建左值参数。 */
  template <typename T> [[nodiscard]] static argument ref(T& value) {
    return from(any_ref::from(value));
  }

  /** @brief 从只读 C++ 对象创建 const 左值参数。 */
  template <typename T> [[nodiscard]] static argument cref(const T& value) {
    return from(any_ref::from_const(value));
  }

  /**
   * @brief 从拥有值创建右值参数。
   *
   * 返回的参数拥有 `value`，调用 `T&&` 或按值参数时允许从内部对象 move。调用方不应复用同一个
   * 右值参数执行多次可能消费参数的调用。
   */
  [[nodiscard]] static argument move(any value) noexcept {
    argument result{};
    result.owned_ = std::move(value);
    result.ref_ = result.owned_.ref();
    result.rvalue_ = true;
    return result;
  }

  [[nodiscard]] bool valid() const noexcept { return ref_.valid() && !consumed_; }
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] meta::type get_type() const noexcept { return ref_.get_type(); }
  [[nodiscard]] bool writable() const noexcept { return ref_.writable(); }
  [[nodiscard]] bool rvalue() const noexcept { return rvalue_; }
  [[nodiscard]] bool consumed() const noexcept { return consumed_; }

  template <typename T> [[nodiscard]] bool matches_type() const noexcept {
    return valid() && ref_.template matches_type<T>();
  }

  template <typename T>
  [[nodiscard]] const std::remove_cv_t<std::remove_reference_t<T>>* try_get() const noexcept {
    return valid() ? ref_.template try_get<T>() : nullptr;
  }

  template <typename T>
  [[nodiscard]] std::remove_cv_t<std::remove_reference_t<T>>* try_get_mutable() const noexcept {
    return valid() ? ref_.template try_get_mutable<T>() : nullptr;
  }

  void consume() const noexcept { consumed_ = true; }

private:
  any_ref ref_{};
  mutable any owned_{};
  bool rvalue_ = false;
  mutable bool consumed_ = false;
};

/** @brief 从已有运行期引用创建左值参数。 */
[[nodiscard]] inline argument arg(any_ref value) noexcept { return argument::from(value); }

/** @brief 从可写 C++ 对象创建左值参数。 */
template <typename T> [[nodiscard]] argument arg(T& value) { return argument::ref(value); }

/** @brief 从只读 C++ 对象创建 const 左值参数。 */
template <typename T> [[nodiscard]] argument carg(const T& value) { return argument::cref(value); }

/** @brief 从拥有值创建右值参数。 */
[[nodiscard]] inline argument move_arg(any value) noexcept {
  return argument::move(std::move(value));
}

/** @brief 从 C++ 值创建拥有值右值参数。当前 `any` 要求值可复制。 */
template <typename T> [[nodiscard]] argument move_arg(T&& value) {
  using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
  return argument::move(any::from(static_cast<const value_type&>(value)));
}

} // namespace caors::meta

namespace caors {

using meta::argument;

} // namespace caors

#endif
