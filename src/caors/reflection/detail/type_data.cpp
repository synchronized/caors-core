// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/reflection/detail/type_data.h"

namespace caors::reflection::detail {

bool type_data::valid() const noexcept {
  if (!ops.valid() || !get_type()) {
    return false;
  }

  for (const field_wrapper& field : fields) {
    if (!field.valid()) {
      return false;
    }
  }

  for (const method_wrapper& method : methods) {
    if (!method.valid()) {
      return false;
    }
  }

  return true;
}

meta::type type_data::get_type() const noexcept {
  return ops.get_type != nullptr ? ops.get_type() : meta::type{};
}

std::optional<field_wrapper> type_data::find_field(core::string_name name) const {
  if (!name) {
    return std::nullopt;
  }

  for (const field_wrapper& field : fields) {
    if (field.name() == name) {
      return field;
    }
  }

  return std::nullopt;
}

std::optional<field_wrapper> type_data::find_field(std::string_view name) const {
  const std::optional<core::string_name> interned_name = core::string_name::find(name);
  if (!interned_name.has_value()) {
    return std::nullopt;
  }

  return find_field(*interned_name);
}

std::optional<method_wrapper> type_data::find_method(core::string_name name) const {
  if (!name) {
    return std::nullopt;
  }

  for (const method_wrapper& method : methods) {
    if (method.name() == name) {
      return method;
    }
  }

  return std::nullopt;
}

std::optional<method_wrapper> type_data::find_method(std::string_view name) const {
  const std::optional<core::string_name> interned_name = core::string_name::find(name);
  if (!interned_name.has_value()) {
    return std::nullopt;
  }

  return find_method(*interned_name);
}

} // namespace caors::reflection::detail
