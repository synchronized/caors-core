// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_REFLECTION_WRAPPER_H_
#define CAORS_REFLECTION_WRAPPER_H_

#include "caors/meta/type.h"
#include "caors/object.h"

#include <functional>
#include <memory>
#include <optional>
#include <type_traits>

namespace caors {

/**
 * @brief C++ wrapper 类型到反射字段语义的映射。
 *
 * 用户自定义 wrapper 可以特化该模板接入反射系统。`inline_value` 表示 wrapper 序列化时按
 * 包装的值处理；`object_ref` 预留给后续 `caors::ref<T>` / `object_ref<T>` 这类对象引用。
 */
template <typename T> struct wrapper_traits {
  static constexpr bool is_wrapper = false;
  static constexpr bool nullable = false;
  static constexpr wrapper_semantics semantics = wrapper_semantics::none;
};

template <typename T, typename Deleter> struct wrapper_traits<std::unique_ptr<T, Deleter>> {
  static constexpr bool is_wrapper = true;
  static constexpr bool nullable = true;
  static constexpr wrapper_semantics semantics = wrapper_semantics::inline_value;
  using wrapped_type = std::remove_cv_t<T>;
};

template <typename T> struct wrapper_traits<std::shared_ptr<T>> {
  static constexpr bool is_wrapper = true;
  static constexpr bool nullable = true;
  static constexpr wrapper_semantics semantics = wrapper_semantics::inline_value;
  using wrapped_type = std::remove_cv_t<T>;
};

template <typename T> struct wrapper_traits<std::weak_ptr<T>> {
  static constexpr bool is_wrapper = true;
  static constexpr bool nullable = true;
  static constexpr wrapper_semantics semantics = wrapper_semantics::inline_value;
  using wrapped_type = std::remove_cv_t<T>;
};

template <typename T> struct wrapper_traits<std::optional<T>> {
  static constexpr bool is_wrapper = true;
  static constexpr bool nullable = true;
  static constexpr wrapper_semantics semantics = wrapper_semantics::inline_value;
  using wrapped_type = std::remove_cv_t<T>;
};

template <typename T> struct wrapper_traits<std::reference_wrapper<T>> {
  static constexpr bool is_wrapper = true;
  static constexpr bool nullable = false;
  static constexpr wrapper_semantics semantics = wrapper_semantics::inline_value;
  using wrapped_type = std::remove_cv_t<T>;
};

template <typename T> struct wrapper_traits<object_ref<T>> {
  static constexpr bool is_wrapper = true;
  static constexpr bool nullable = true;
  static constexpr wrapper_semantics semantics = wrapper_semantics::object_ref;
  using wrapped_type = std::remove_cv_t<T>;
};

} // namespace caors

#endif
