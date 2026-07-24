// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_METHOD_ACCESS_H_
#define CAORS_META_DETAIL_METHOD_ACCESS_H_

#include "caors/meta/method.h"

namespace caors::meta::detail {

/** @brief `meta::method` 和 `meta::method_set` 的内部构造访问口。 */
class method_access final {
public:
  [[nodiscard]] static method make(const method_record* data) noexcept { return method{data}; }

  [[nodiscard]] static method_set make_set(const method_set_record* data) noexcept {
    return method_set{data};
  }
};

} // namespace caors::meta::detail

#endif
