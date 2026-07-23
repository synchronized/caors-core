// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta/detail/pending_type_record.h"

namespace caors::meta::detail {

error pending_type_record::validate() const noexcept {
  if (!data_.valid() || !data_.id() || !data_.name() || !data_.value_ops().valid() ||
      !data_.members_valid()) {
    return {error_domain::meta, errc::invalid_type};
  }

  const parent_cast_ops* cast = data_.parent_cast();
  if (cast != nullptr && (!data_.parent_id() || cast->parent != data_.parent_id())) {
    return {error_domain::meta, errc::invalid_type};
  }

  return {};
}

} // namespace caors::meta::detail
