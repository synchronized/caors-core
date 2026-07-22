// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_PROPERTY_ACCESS_H_
#define CAORS_META_DETAIL_PROPERTY_ACCESS_H_

#include "caors/meta/property.h"

namespace caors::meta::detail {

/** @brief `meta::property` 的内部构造访问口。 */
class property_access final {
public:
  [[nodiscard]] static property make(const property_data* data) noexcept { return property{data}; }
};

} // namespace caors::meta::detail

#endif
