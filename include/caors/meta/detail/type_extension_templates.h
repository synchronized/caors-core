// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_TYPE_EXTENSION_TEMPLATES_H_
#define CAORS_META_DETAIL_TYPE_EXTENSION_TEMPLATES_H_

#include "caors/meta/detail/extension_data.h"
#include "caors/meta/detail/type_access.h"

#include <type_traits>

namespace caors::meta::detail {

/** @brief 从已注册类型视图直达查询指定 extension。 */
template <typename Extension>
[[nodiscard]] const Extension* find_type_extension(const type& source) noexcept {
  using extension_type = std::decay_t<Extension>;

  const type_data* data = type_access::registered_data_of(source);
  if (data == nullptr) {
    return nullptr;
  }

  const extension_data* extension = data->find_extension(extension_key_of<extension_type>());
  return extension != nullptr ? extension_value<extension_type>(*extension) : nullptr;
}

} // namespace caors::meta::detail

#endif
