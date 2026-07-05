// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_ADAPTERS_STL_WRAPPER_H_
#define CAORS_ADAPTERS_STL_WRAPPER_H_

#include "caors/config.h"
#include "caors/traits/wrapper.h"

#include <functional>
#include <memory>
#include <new>
#include <optional>
#include <type_traits>

namespace caors::traits {

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

  static bool ensure_value(std::unique_ptr<T, Deleter>& source) {
    using pointer_type = typename std::unique_ptr<T, Deleter>::pointer;

    if (source) {
      return true;
    }

    if constexpr (std::is_default_constructible_v<wrapped_type> &&
                  std::is_convertible_v<wrapped_type*, pointer_type>) {
#if CAORS_HAS_EXCEPTIONS
      try {
        source.reset(new wrapped_type());
      } catch (...) {
        return false;
      }
#else
      if constexpr (!std::is_nothrow_default_constructible_v<wrapped_type>) {
        return false;
      } else {
        wrapped_type* value = new (std::nothrow) wrapped_type();
        if (value == nullptr) {
          return false;
        }
        source.reset(value);
      }
#endif
      return static_cast<bool>(source);
    } else {
      return false;
    }
  }

  static bool set_value(std::unique_ptr<T, Deleter>& source, wrapped_type* value) noexcept {
    using pointer_type = typename std::unique_ptr<T, Deleter>::pointer;
    if (value == nullptr) {
      return false;
    }

    if constexpr (std::is_convertible_v<wrapped_type*, pointer_type>) {
      source.reset(value);
      return static_cast<bool>(source);
    } else {
      return false;
    }
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

  static bool ensure_value(std::shared_ptr<T>& source) {
    if (source) {
      return true;
    }

    if constexpr (std::is_default_constructible_v<wrapped_type>) {
#if CAORS_HAS_EXCEPTIONS
      try {
        source = std::make_shared<wrapped_type>();
      } catch (...) {
        return false;
      }
#else
      return false;
#endif
      return static_cast<bool>(source);
    } else {
      return false;
    }
  }

  static bool set_value(std::shared_ptr<T>& source, wrapped_type* value) {
    if (value == nullptr) {
      return false;
    }

#if CAORS_HAS_EXCEPTIONS
    try {
      source.reset(value);
    } catch (...) {
      return false;
    }
    return static_cast<bool>(source);
#else
    (void)source;
    (void)value;
    return false;
#endif
  }
};

/** @brief `std::weak_ptr` 只标记为不支持的 wrapper，不默认暴露内部值。 */
template <typename T> struct wrapper_traits<std::weak_ptr<T>> {
  static constexpr bool is_wrapper = true;
  static constexpr bool nullable = true;
  static constexpr wrapper_semantics semantics = wrapper_semantics::unsupported;
  using wrapped_type = std::remove_cv_t<T>;
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

  static bool ensure_value(std::optional<T>& source) {
    if (source.has_value()) {
      return true;
    }

    if constexpr (std::is_default_constructible_v<wrapped_type>) {
#if CAORS_HAS_EXCEPTIONS
      try {
        source.emplace();
      } catch (...) {
        return false;
      }
#else
      if constexpr (!std::is_nothrow_default_constructible_v<wrapped_type>) {
        return false;
      } else {
        source.emplace();
      }
#endif
      return source.has_value();
    } else {
      return false;
    }
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
  static bool ensure_value(std::reference_wrapper<T>&) { return true; }
};

} // namespace caors::traits

#endif
