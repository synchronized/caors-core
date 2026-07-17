// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta/detail/type_data.h"

namespace caors::meta::detail {

bool type_data::members_valid() const noexcept {
  for (const property_data& property : properties_) {
    if (!property.valid()) {
      return false;
    }
  }

  for (const method_data& method : methods_) {
    if (!method.valid()) {
      return false;
    }
  }

  for (const enum_value_data& value : enum_values_) {
    if (!value.valid()) {
      return false;
    }
  }

  return true;
}

std::size_t type_data::property_count() const noexcept { return properties_.size(); }

const property_data* type_data::property_at(std::size_t index) const noexcept {
  return index < properties_.size() ? &properties_[index] : nullptr;
}

property_data* type_data::mutable_property_at(std::size_t index) noexcept {
  return index < properties_.size() ? &properties_[index] : nullptr;
}

const property_data* type_data::find_property_ptr(core::string_name name) const noexcept {
  if (!name) {
    return nullptr;
  }

  for (const property_data& property : properties_) {
    if (property.name() == name) {
      return &property;
    }
  }

  return nullptr;
}

property_data* type_data::mutable_find_property_ptr(core::string_name name) noexcept {
  if (!name) {
    return nullptr;
  }

  for (property_data& property : properties_) {
    if (property.name() == name) {
      return &property;
    }
  }

  return nullptr;
}

std::optional<property_data> type_data::find_property(core::string_name name) const {
  const property_data* property = find_property_ptr(name);
  if (property != nullptr) {
    return *property;
  }

  return std::nullopt;
}

std::optional<property_data> type_data::find_property(std::string_view name) const {
  const std::optional<core::string_name> interned_name = core::string_name::find(name);
  if (!interned_name.has_value()) {
    return std::nullopt;
  }

  return find_property(*interned_name);
}

void type_data::add_property(property_data property) { properties_.push_back(property); }

std::size_t type_data::method_count() const noexcept { return methods_.size(); }

const method_data* type_data::method_at(std::size_t index) const noexcept {
  return index < methods_.size() ? &methods_[index] : nullptr;
}

const method_data* type_data::find_method_ptr(core::string_name name) const noexcept {
  if (!name) {
    return nullptr;
  }

  for (const method_data& method : methods_) {
    if (method.name() == name) {
      return &method;
    }
  }

  return nullptr;
}

method_data* type_data::mutable_find_method_ptr(core::string_name name) noexcept {
  if (!name) {
    return nullptr;
  }

  for (method_data& method : methods_) {
    if (method.name() == name) {
      return &method;
    }
  }

  return nullptr;
}

std::optional<method_data> type_data::find_method(core::string_name name) const {
  const method_data* method = find_method_ptr(name);
  if (method != nullptr) {
    return *method;
  }

  return std::nullopt;
}

std::optional<method_data> type_data::find_method(std::string_view name) const {
  const std::optional<core::string_name> interned_name = core::string_name::find(name);
  if (!interned_name.has_value()) {
    return std::nullopt;
  }

  return find_method(*interned_name);
}

void type_data::add_method(method_data method) { methods_.push_back(method); }

std::size_t type_data::enum_value_count() const noexcept { return enum_values_.size(); }

const enum_value_data* type_data::enum_value_at(std::size_t index) const noexcept {
  return index < enum_values_.size() ? &enum_values_[index] : nullptr;
}

const enum_value_data* type_data::find_enum_value_ptr(core::string_name name) const noexcept {
  if (!name) {
    return nullptr;
  }

  for (const enum_value_data& value : enum_values_) {
    if (value.name() == name) {
      return &value;
    }
  }

  return nullptr;
}

enum_value_data* type_data::mutable_find_enum_value_ptr(core::string_name name) noexcept {
  if (!name) {
    return nullptr;
  }

  for (enum_value_data& value : enum_values_) {
    if (value.name() == name) {
      return &value;
    }
  }

  return nullptr;
}

const enum_value_data* type_data::find_enum_value_ptr(std::int64_t value) const noexcept {
  for (const enum_value_data& enum_value : enum_values_) {
    if (enum_value.to_i64() == value) {
      return &enum_value;
    }
  }

  return nullptr;
}

const enum_value_data* type_data::find_enum_value_ptr(std::uint64_t value) const noexcept {
  for (const enum_value_data& enum_value : enum_values_) {
    if (enum_value.to_u64() == value) {
      return &enum_value;
    }
  }

  return nullptr;
}

void type_data::add_enum_value(enum_value_data value) { enum_values_.push_back(std::move(value)); }

} // namespace caors::meta::detail
