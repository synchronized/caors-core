// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_BUILTIN_ENUM_OPS_H_
#define CAORS_META_DETAIL_BUILTIN_ENUM_OPS_H_

#include "caors/meta/detail/builtin/capability.h"

#include <cstdint>
#include <limits>
#include <type_traits>

namespace caors::meta::detail {

using enum_to_i64_fn = std::int64_t (*)(const void*) noexcept;
using enum_to_u64_fn = std::uint64_t (*)(const void*) noexcept;
using enum_from_i64_fn = bool (*)(void*, std::int64_t) noexcept;
using enum_from_u64_fn = bool (*)(void*, std::uint64_t) noexcept;

/** @brief 枚举类型和整数之间的转换函数表。 */
struct enum_ops {
  enum_to_i64_fn to_i64 = nullptr;
  enum_to_u64_fn to_u64 = nullptr;
  enum_from_i64_fn from_i64 = nullptr;
  enum_from_u64_fn from_u64 = nullptr;
  bool is_signed = false;

  [[nodiscard]] bool valid() const noexcept {
    return to_i64 != nullptr && to_u64 != nullptr && from_i64 != nullptr && from_u64 != nullptr;
  }
};

template <typename T> struct enum_ops_model {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  using underlying_type = std::underlying_type_t<type>;

  [[nodiscard]] static std::int64_t to_i64(const void* storage) noexcept {
    return static_cast<std::int64_t>(
        static_cast<underlying_type>(*static_cast<const type*>(storage)));
  }

  [[nodiscard]] static std::uint64_t to_u64(const void* storage) noexcept {
    return static_cast<std::uint64_t>(
        static_cast<underlying_type>(*static_cast<const type*>(storage)));
  }

  [[nodiscard]] static bool from_i64(void* storage, std::int64_t value) noexcept {
    if (storage == nullptr) {
      return false;
    }

    if constexpr (std::is_signed_v<underlying_type>) {
      if (value < static_cast<std::int64_t>(std::numeric_limits<underlying_type>::min()) ||
          value > static_cast<std::int64_t>(std::numeric_limits<underlying_type>::max())) {
        return false;
      }
    } else {
      if (value < 0 ||
          static_cast<std::uint64_t>(value) >
              static_cast<std::uint64_t>(std::numeric_limits<underlying_type>::max())) {
        return false;
      }
    }

    *static_cast<type*>(storage) = static_cast<type>(static_cast<underlying_type>(value));
    return true;
  }

  [[nodiscard]] static bool from_u64(void* storage, std::uint64_t value) noexcept {
    if (storage == nullptr ||
        value > static_cast<std::uint64_t>(std::numeric_limits<underlying_type>::max())) {
      return false;
    }

    *static_cast<type*>(storage) = static_cast<type>(static_cast<underlying_type>(value));
    return true;
  }

  inline static const enum_ops value{
      &to_i64, &to_u64, &from_i64, &from_u64, std::is_signed_v<underlying_type>,
  };
};

template <typename T> [[nodiscard]] const enum_ops* enum_ops_ptr_of() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (std::is_enum_v<type>) {
    return enum_ops_model<type>::value.valid() ? &enum_ops_model<type>::value : nullptr;
  } else {
    return nullptr;
  }
}

} // namespace caors::meta::detail

#endif
