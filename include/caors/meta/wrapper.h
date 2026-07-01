// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_WRAPPER_H_
#define CAORS_META_WRAPPER_H_

#include "caors/meta/type.h"

#include <functional>
#include <memory>
#include <optional>
#include <type_traits>

namespace caors {

/** @brief C++ wrapper 类型到 meta 类型语义的映射。 */
template <typename T> struct wrapper_traits {
  static constexpr bool is_wrapper = false;
  static constexpr bool nullable = false;
  static constexpr wrapper_semantics semantics = wrapper_semantics::none;
};

/** @brief wrapper 类型的内部值访问适配。 */
template <typename T> struct wrapper_access {
  using wrapped_type = void;

  [[nodiscard]] static const void* value(const T&) noexcept { return nullptr; }
  [[nodiscard]] static void* mutable_value(T&) noexcept { return nullptr; }
  [[nodiscard]] static bool has_value(const T&) noexcept { return false; }
  static bool reset(T&) noexcept { return false; }
};

template <typename T, typename Deleter> struct wrapper_traits<std::unique_ptr<T, Deleter>> {
  static constexpr bool is_wrapper = true;
  static constexpr bool nullable = true;
  static constexpr wrapper_semantics semantics = wrapper_semantics::inline_value;
  using wrapped_type = std::remove_cv_t<T>;
};

template <typename T, typename Deleter> struct wrapper_access<std::unique_ptr<T, Deleter>> {
  using wrapped_type = std::remove_cv_t<T>;

  [[nodiscard]] static const wrapped_type*
  value(const std::unique_ptr<T, Deleter>& source) noexcept {
    return source.get();
  }

  [[nodiscard]] static wrapped_type* mutable_value(std::unique_ptr<T, Deleter>& source) noexcept {
    return source.get();
  }

  [[nodiscard]] static bool has_value(const std::unique_ptr<T, Deleter>& source) noexcept {
    return static_cast<bool>(source);
  }

  static bool reset(std::unique_ptr<T, Deleter>& source) noexcept {
    source.reset();
    return true;
  }
};

template <typename T> struct wrapper_traits<std::shared_ptr<T>> {
  static constexpr bool is_wrapper = true;
  static constexpr bool nullable = true;
  static constexpr wrapper_semantics semantics = wrapper_semantics::inline_value;
  using wrapped_type = std::remove_cv_t<T>;
};

template <typename T> struct wrapper_access<std::shared_ptr<T>> {
  using wrapped_type = std::remove_cv_t<T>;

  [[nodiscard]] static const wrapped_type* value(const std::shared_ptr<T>& source) noexcept {
    return source.get();
  }

  [[nodiscard]] static wrapped_type* mutable_value(std::shared_ptr<T>& source) noexcept {
    return source.get();
  }

  [[nodiscard]] static bool has_value(const std::shared_ptr<T>& source) noexcept {
    return static_cast<bool>(source);
  }

  static bool reset(std::shared_ptr<T>& source) noexcept {
    source.reset();
    return true;
  }
};

template <typename T> struct wrapper_traits<std::weak_ptr<T>> {
  static constexpr bool is_wrapper = true;
  static constexpr bool nullable = true;
  static constexpr wrapper_semantics semantics = wrapper_semantics::inline_value;
  using wrapped_type = std::remove_cv_t<T>;
};

template <typename T> struct wrapper_access<std::weak_ptr<T>> {
  using wrapped_type = std::remove_cv_t<T>;

  [[nodiscard]] static const wrapped_type* value(const std::weak_ptr<T>&) noexcept {
    return nullptr;
  }

  [[nodiscard]] static wrapped_type* mutable_value(std::weak_ptr<T>&) noexcept { return nullptr; }

  [[nodiscard]] static bool has_value(const std::weak_ptr<T>& source) noexcept {
    return !source.expired();
  }

  static bool reset(std::weak_ptr<T>& source) noexcept {
    source.reset();
    return true;
  }
};

template <typename T> struct wrapper_traits<std::optional<T>> {
  static constexpr bool is_wrapper = true;
  static constexpr bool nullable = true;
  static constexpr wrapper_semantics semantics = wrapper_semantics::inline_value;
  using wrapped_type = std::remove_cv_t<T>;
};

template <typename T> struct wrapper_access<std::optional<T>> {
  using wrapped_type = std::remove_cv_t<T>;

  [[nodiscard]] static const wrapped_type* value(const std::optional<T>& source) noexcept {
    return source.has_value() ? std::addressof(*source) : nullptr;
  }

  [[nodiscard]] static wrapped_type* mutable_value(std::optional<T>& source) noexcept {
    return source.has_value() ? std::addressof(*source) : nullptr;
  }

  [[nodiscard]] static bool has_value(const std::optional<T>& source) noexcept {
    return source.has_value();
  }

  static bool reset(std::optional<T>& source) noexcept {
    source.reset();
    return true;
  }
};

template <typename T> struct wrapper_traits<std::reference_wrapper<T>> {
  static constexpr bool is_wrapper = true;
  static constexpr bool nullable = false;
  static constexpr wrapper_semantics semantics = wrapper_semantics::inline_value;
  using wrapped_type = std::remove_cv_t<T>;
};

template <typename T> struct wrapper_access<std::reference_wrapper<T>> {
  using wrapped_type = std::remove_cv_t<T>;

  [[nodiscard]] static const wrapped_type* value(const std::reference_wrapper<T>& source) noexcept {
    return std::addressof(source.get());
  }

  [[nodiscard]] static wrapped_type* mutable_value(std::reference_wrapper<T>& source) noexcept {
    return std::addressof(source.get());
  }

  [[nodiscard]] static bool has_value(const std::reference_wrapper<T>&) noexcept { return true; }
  static bool reset(std::reference_wrapper<T>&) noexcept { return false; }
};

} // namespace caors

#endif
