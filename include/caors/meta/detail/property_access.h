// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_PROPERTY_ACCESS_H_
#define CAORS_META_DETAIL_PROPERTY_ACCESS_H_

#include "caors/meta/property.h"

namespace caors::meta::detail {

/** @brief `meta::property` 的内部构造访问口。 */
class property_access final {
public:
  [[nodiscard]] static property make(const property_record* data,
                                     std::shared_ptr<registry_layer_state> layer = {}) noexcept {
    return property{data, std::move(layer)};
  }

  [[nodiscard]] static const property_record* data_of(const property& value) noexcept {
    return value.info();
  }
};

} // namespace caors::meta::detail

#endif
