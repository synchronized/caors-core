// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_BUILTIN_SCALAR_OPS_MODEL_H_
#define CAORS_META_DETAIL_BUILTIN_SCALAR_OPS_MODEL_H_

#include "caors/config.h"
#include "caors/core/containers.h"
#include "caors/core/string_name.h"
#include "caors/meta/ops/scalar_ops.h"

#include <limits>
#include <string>
#include <string_view>
#include <type_traits>

namespace caors::meta::detail {

template <typename T> using scalar_type = std::remove_cv_t<std::remove_reference_t<T>>;

template <typename T>
inline constexpr bool scalar_string_v =
    std::is_same_v<scalar_type<T>, core::string> || std::is_same_v<scalar_type<T>, std::string> ||
    std::is_same_v<scalar_type<T>, std::string_view> ||
    std::is_same_v<scalar_type<T>, core::string_name>;

template <typename T>
inline constexpr bool scalar_supported_v =
    std::is_same_v<scalar_type<T>, bool> ||
    (std::is_integral_v<scalar_type<T>> && !std::is_same_v<scalar_type<T>, bool>) ||
    std::is_floating_point_v<scalar_type<T>> || scalar_string_v<T>;

template <typename T, typename = void> struct scalar_ops_model {
  inline static const scalar_ops value{};
};

template <typename T>
struct scalar_ops_model<T, std::enable_if_t<std::is_same_v<scalar_type<T>, bool>>> {
  using type = scalar_type<T>;

  [[nodiscard]] static bool to_bool(const void* object, bool& value) noexcept {
    value = *static_cast<const type*>(object);
    return true;
  }

  [[nodiscard]] static bool from_bool(void* object, bool value) noexcept {
    *static_cast<type*>(object) = value;
    return true;
  }

  inline static const scalar_ops value{
      scalar_kind::boolean,
      sizeof(type),
      false,
      false,
      &to_bool,
      nullptr,
      nullptr,
      nullptr,
      nullptr,
      &from_bool,
      nullptr,
      nullptr,
      nullptr,
      nullptr,
  };
};

template <typename T>
struct scalar_ops_model<T, std::enable_if_t<std::is_same_v<scalar_type<T>, char>>> {
  using type = scalar_type<T>;

  [[nodiscard]] static bool to_string(const void* object, std::string_view& value) noexcept {
    value = std::string_view{static_cast<const char*>(object), 1};
    return true;
  }

  [[nodiscard]] static bool from_string(void* object, std::string_view value) noexcept {
    if (value.size() != 1) {
      return false;
    }

    *static_cast<type*>(object) = value[0];
    return true;
  }

  inline static const scalar_ops value{
      scalar_kind::character,
      sizeof(type),
      false,
      false,
      nullptr,
      nullptr,
      nullptr,
      nullptr,
      &to_string,
      nullptr,
      nullptr,
      nullptr,
      nullptr,
      &from_string,
  };
};

template <typename T>
struct scalar_ops_model<
    T,
    std::enable_if_t<std::is_integral_v<scalar_type<T>> && !std::is_same_v<scalar_type<T>, bool> &&
                     !std::is_same_v<scalar_type<T>, char> && std::is_signed_v<scalar_type<T>>>> {
  using type = scalar_type<T>;

  [[nodiscard]] static bool to_i64(const void* object, std::int64_t& value) noexcept {
    value = static_cast<std::int64_t>(*static_cast<const type*>(object));
    return true;
  }

  [[nodiscard]] static bool from_i64(void* object, std::int64_t value) noexcept {
    if (value < static_cast<std::int64_t>(std::numeric_limits<type>::min()) ||
        value > static_cast<std::int64_t>(std::numeric_limits<type>::max())) {
      return false;
    }

    *static_cast<type*>(object) = static_cast<type>(value);
    return true;
  }

  [[nodiscard]] static bool from_u64(void* object, std::uint64_t value) noexcept {
    if (value > static_cast<std::uint64_t>(std::numeric_limits<type>::max())) {
      return false;
    }

    *static_cast<type*>(object) = static_cast<type>(value);
    return true;
  }

  inline static const scalar_ops value{
      scalar_kind::signed_integer,
      sizeof(type),
      true,
      false,
      nullptr,
      &to_i64,
      nullptr,
      nullptr,
      nullptr,
      nullptr,
      &from_i64,
      &from_u64,
      nullptr,
      nullptr,
  };
};

template <typename T>
struct scalar_ops_model<
    T,
    std::enable_if_t<std::is_integral_v<scalar_type<T>> && !std::is_same_v<scalar_type<T>, bool> &&
                     !std::is_same_v<scalar_type<T>, char> && !std::is_signed_v<scalar_type<T>>>> {
  using type = scalar_type<T>;

  [[nodiscard]] static bool to_u64(const void* object, std::uint64_t& value) noexcept {
    value = static_cast<std::uint64_t>(*static_cast<const type*>(object));
    return true;
  }

  [[nodiscard]] static bool from_i64(void* object, std::int64_t value) noexcept {
    if (value < 0 || static_cast<std::uint64_t>(value) >
                         static_cast<std::uint64_t>(std::numeric_limits<type>::max())) {
      return false;
    }

    *static_cast<type*>(object) = static_cast<type>(value);
    return true;
  }

  [[nodiscard]] static bool from_u64(void* object, std::uint64_t value) noexcept {
    if (value > static_cast<std::uint64_t>(std::numeric_limits<type>::max())) {
      return false;
    }

    *static_cast<type*>(object) = static_cast<type>(value);
    return true;
  }

  inline static const scalar_ops value{
      scalar_kind::unsigned_integer,
      sizeof(type),
      false,
      false,
      nullptr,
      nullptr,
      &to_u64,
      nullptr,
      nullptr,
      nullptr,
      &from_i64,
      &from_u64,
      nullptr,
      nullptr,
  };
};

template <typename T>
struct scalar_ops_model<T, std::enable_if_t<std::is_floating_point_v<scalar_type<T>>>> {
  using type = scalar_type<T>;

  [[nodiscard]] static bool to_f64(const void* object, double& value) noexcept {
    value = static_cast<double>(*static_cast<const type*>(object));
    return true;
  }

  [[nodiscard]] static bool from_i64(void* object, std::int64_t value) noexcept {
    *static_cast<type*>(object) = static_cast<type>(value);
    return true;
  }

  [[nodiscard]] static bool from_u64(void* object, std::uint64_t value) noexcept {
    *static_cast<type*>(object) = static_cast<type>(value);
    return true;
  }

  [[nodiscard]] static bool from_f64(void* object, double value) noexcept {
    *static_cast<type*>(object) = static_cast<type>(value);
    return true;
  }

  inline static const scalar_ops value{
      scalar_kind::floating_point,
      sizeof(type),
      true,
      false,
      nullptr,
      nullptr,
      nullptr,
      &to_f64,
      nullptr,
      nullptr,
      &from_i64,
      &from_u64,
      &from_f64,
      nullptr,
  };
};

template <typename T> struct scalar_ops_model<T, std::enable_if_t<scalar_string_v<T>>> {
  using type = scalar_type<T>;

  [[nodiscard]] static bool to_string(const void* object, std::string_view& value) noexcept {
    if constexpr (std::is_same_v<type, core::string> || std::is_same_v<type, std::string>) {
      const auto& text = *static_cast<const type*>(object);
      value = std::string_view{text.data(), text.size()};
    } else if constexpr (std::is_same_v<type, std::string_view>) {
      value = *static_cast<const type*>(object);
    } else {
      value = static_cast<const core::string_name*>(object)->view();
    }
    return true;
  }

  [[nodiscard]] static bool from_string(void* object, std::string_view value) {
#if CAORS_HAS_EXCEPTIONS
    try {
#endif
      if constexpr (std::is_same_v<type, core::string> || std::is_same_v<type, std::string>) {
        static_cast<type*>(object)->assign(value.data(), value.size());
      } else if constexpr (std::is_same_v<type, std::string_view>) {
        *static_cast<type*>(object) = value;
      } else {
        *static_cast<type*>(object) = core::string_name{value};
      }

      return true;
#if CAORS_HAS_EXCEPTIONS
    } catch (...) {
      return false;
    }
#endif
  }

  inline static const scalar_ops value{
      scalar_kind::string,
      sizeof(type),
      false,
      std::is_same_v<type, std::string_view>,
      nullptr,
      nullptr,
      nullptr,
      nullptr,
      &to_string,
      nullptr,
      nullptr,
      nullptr,
      nullptr,
      &from_string,
  };
};

template <typename T> [[nodiscard]] const scalar_ops* scalar_ops_ptr_of() noexcept {
  using type = scalar_type<T>;
  if constexpr (scalar_supported_v<type>) {
    return &scalar_ops_model<type>::value;
  } else {
    return nullptr;
  }
}

} // namespace caors::meta::detail

#endif
