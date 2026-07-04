// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/reflection/detail/type_data.h"

namespace caors::reflection::detail {

bool type_data::valid() const noexcept {
  for (const field_data& field : fields) {
    if (!field.valid()) {
      return false;
    }
  }

  return true;
}

std::optional<field_data> type_data::find_field(core::string_name name) const {
  if (!name) {
    return std::nullopt;
  }

  for (const field_data& field : fields) {
    if (field.name == name) {
      return field;
    }
  }

  return std::nullopt;
}

std::optional<field_data> type_data::find_field(std::string_view name) const {
  const std::optional<core::string_name> interned_name = core::string_name::find(name);
  if (!interned_name.has_value()) {
    return std::nullopt;
  }

  return find_field(*interned_name);
}

} // namespace caors::reflection::detail
