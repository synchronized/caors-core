// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_BUILTIN_ENUM_OPS_H_
#define CAORS_META_DETAIL_BUILTIN_ENUM_OPS_H_

#include "caors/meta/detail/builtin/capability.h"

#include <cstdint>
#include <limits>
#include <type_traits>

namespace caors::meta::detail {

template <typename Enum> [[nodiscard]] std::int64_t enum_to_i64(const void* storage) noexcept {
  using underlying_type = std::underlying_type_t<Enum>;
  return static_cast<std::int64_t>(
      static_cast<underlying_type>(*static_cast<const Enum*>(storage)));
}

template <typename Enum> [[nodiscard]] std::uint64_t enum_to_u64(const void* storage) noexcept {
  using underlying_type = std::underlying_type_t<Enum>;
  return static_cast<std::uint64_t>(
      static_cast<underlying_type>(*static_cast<const Enum*>(storage)));
}

template <typename Enum>
[[nodiscard]] bool enum_from_i64(void* storage, std::int64_t value) noexcept {
  using underlying_type = std::underlying_type_t<Enum>;
  if (storage == nullptr) {
    return false;
  }

  if constexpr (std::is_signed_v<underlying_type>) {
    if (value < static_cast<std::int64_t>(std::numeric_limits<underlying_type>::min()) ||
        value > static_cast<std::int64_t>(std::numeric_limits<underlying_type>::max())) {
      return false;
    }
  } else {
    if (value < 0 || static_cast<std::uint64_t>(value) >
                         static_cast<std::uint64_t>(std::numeric_limits<underlying_type>::max())) {
      return false;
    }
  }

  *static_cast<Enum*>(storage) = static_cast<Enum>(static_cast<underlying_type>(value));
  return true;
}

template <typename Enum>
[[nodiscard]] bool enum_from_u64(void* storage, std::uint64_t value) noexcept {
  using underlying_type = std::underlying_type_t<Enum>;
  if (storage == nullptr ||
      value > static_cast<std::uint64_t>(std::numeric_limits<underlying_type>::max())) {
    return false;
  }

  *static_cast<Enum*>(storage) = static_cast<Enum>(static_cast<underlying_type>(value));
  return true;
}

template <typename T> [[nodiscard]] enum_ops make_enum_ops() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (std::is_enum_v<type>) {
    using underlying_type = std::underlying_type_t<type>;
    return enum_ops{
        &enum_to_i64<type>,
        &enum_to_u64<type>,
        &enum_from_i64<type>,
        &enum_from_u64<type>,
        std::is_signed_v<underlying_type>,
    };
  } else {
    return {};
  }
}

template <typename T> struct enum_ops_model {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;

  inline static const enum_ops value = make_enum_ops<type>();
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
