// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta/detail/pending_type_record.h"

#include "caors/meta/detail/type_descriptor.h"

namespace caors::meta::detail {

error pending_type_record::validate() const noexcept {
  const type_descriptor* descriptor = data_.descriptor();
  if (!data_.valid() || !data_.id() || !data_.name() || descriptor == nullptr ||
      !descriptor->valid() || descriptor->type != data_.id() || !data_.members_valid()) {
    return {error_domain::meta, errc::invalid_type};
  }

  const parent_cast_ops* cast = data_.parent_cast();
  if (cast != nullptr && (!data_.parent_id() || cast->parent != data_.parent_id())) {
    return {error_domain::meta, errc::invalid_type};
  }

  return {};
}

} // namespace caors::meta::detail
