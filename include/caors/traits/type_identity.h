// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_TRAITS_TYPE_IDENTITY_H_
#define CAORS_TRAITS_TYPE_IDENTITY_H_

#include "caors/core/type_guid.h"
#include "caors/traits/type_name.h"

#include <string_view>
#include <type_traits>

namespace caors::traits {

/**
 * @brief C++ 类型的静态名称和稳定 GUID 定制点。
 *
 * 默认名称复用 `type_name<T>`，默认 GUID 为空。模块可以对第二模板参数做受约束偏特化，
 * 用户类型也可以直接特化本 traits。
 */
template <typename T, typename = void> struct type_identity {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;

  [[nodiscard]] static std::string_view name() noexcept {
    if constexpr (has_type_name_v<type>) {
      return type_name<type>::value;
    } else {
      return {};
    }
  }

  [[nodiscard]] static constexpr core::type_guid guid() noexcept { return core::type_guid::nil(); }
};

} // namespace caors::traits

#endif
