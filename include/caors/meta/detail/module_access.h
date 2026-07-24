// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_MODULE_ACCESS_H_
#define CAORS_META_DETAIL_MODULE_ACCESS_H_

#include "caors/meta/detail/module_record.h"
#include "caors/meta/module.h"

namespace caors::meta::detail {

/** @brief `meta::module_view` 的内部访问口。 */
class module_access final {
public:
  [[nodiscard]] static module_view make(const module_record* data) noexcept {
    return module_view{data};
  }

  [[nodiscard]] static const module_record* data_of(const module_view& view) noexcept {
    return view.data_ != nullptr && view.data_->valid() ? view.data_ : nullptr;
  }
};

} // namespace caors::meta::detail

#endif
