// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_ENUM_VALUE_ACCESS_H_
#define CAORS_META_DETAIL_ENUM_VALUE_ACCESS_H_

#include "caors/meta/enum_value.h"

namespace caors::meta::detail {

/** @brief `meta::enum_value_view` 的内部构造访问口。 */
class enum_value_access final {
public:
  [[nodiscard]] static enum_value_view make(const enum_value_data* data) noexcept {
    return enum_value_view{data};
  }
};

} // namespace caors::meta::detail

#endif
