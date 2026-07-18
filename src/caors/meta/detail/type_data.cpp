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

  for (const method_set_data& method_set : methods_) {
    if (!method_set.valid()) {
      return false;
    }
  }

  for (const enum_value_data& value : enum_values_) {
    if (!value.valid()) {
      return false;
    }
  }

  for (const capability_data& capability : capabilities_) {
    if (!capability.valid()) {
      return false;
    }
  }

  for (const extension_data& extension : extensions_) {
    if (!extension.valid()) {
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

const method_set_data* type_data::method_at(std::size_t index) const noexcept {
  return index < methods_.size() ? &methods_[index] : nullptr;
}

const method_set_data* type_data::find_method_ptr(core::string_name name) const noexcept {
  if (!name) {
    return nullptr;
  }

  for (const method_set_data& method_set : methods_) {
    if (method_set.name() == name) {
      return &method_set;
    }
  }

  return nullptr;
}

method_set_data* type_data::mutable_find_method_ptr(core::string_name name) noexcept {
  if (!name) {
    return nullptr;
  }

  for (method_set_data& method_set : methods_) {
    if (method_set.name() == name) {
      return &method_set;
    }
  }

  return nullptr;
}

std::optional<std::size_t> type_data::add_method(method_data method) {
  if (!method.valid() || !method.name()) {
    return std::nullopt;
  }

  method_set_data* existing = mutable_find_method_ptr(method.name());
  if (existing != nullptr) {
    return existing->add_overload(std::move(method));
  }

  method_set_data method_set{method.name()};
  std::optional<std::size_t> overload_index = method_set.add_overload(std::move(method));
  if (!overload_index.has_value()) {
    return std::nullopt;
  }

  methods_.push_back(std::move(method_set));
  return overload_index;
}

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

const void* type_data::find_capability(capability_key key) const noexcept {
  if (key == nullptr) {
    return nullptr;
  }

  for (const capability_data& capability : capabilities_) {
    if (capability.key == key) {
      return capability.data;
    }
  }

  return nullptr;
}

bool type_data::contains_capability(capability_key key) const noexcept {
  return find_capability(key) != nullptr;
}

std::size_t type_data::capability_count() const noexcept { return capabilities_.size(); }

error type_data::add_capability(capability_key key, const void* capability) {
  if (key == nullptr || capability == nullptr) {
    return {error_domain::meta, errc::invalid_argument};
  }

  if (contains_capability(key)) {
    return {error_domain::meta, errc::duplicate_component};
  }

  capability_data data{};
  data.key = key;
  data.data = capability;
  capabilities_.push_back(std::move(data));
  return {};
}

error type_data::add_owned_capability(capability_key key,
                                      memory::unique_ptr<owned_capability_base> capability) {
  if (key == nullptr || capability == nullptr || capability->data() == nullptr) {
    return {error_domain::meta, errc::invalid_argument};
  }

  if (contains_capability(key)) {
    return {error_domain::meta, errc::duplicate_component};
  }

  capability_data data{};
  data.key = key;
  data.data = capability->data();
  data.owned = std::move(capability);
  capabilities_.push_back(std::move(data));
  return {};
}

bool type_data::contains_extension(extension_key key) const noexcept {
  return find_extension(key) != nullptr;
}

std::size_t type_data::extension_count() const noexcept { return extensions_.size(); }

extension_key type_data::extension_key_at(std::size_t index) const noexcept {
  return index < extensions_.size() ? extensions_[index].key : nullptr;
}

const extension_data* type_data::find_extension(extension_key key) const noexcept {
  if (key == nullptr) {
    return nullptr;
  }

  for (const extension_data& extension : extensions_) {
    if (extension.key == key) {
      return &extension;
    }
  }

  return nullptr;
}

extension_data* type_data::mutable_find_extension(extension_key key) noexcept {
  if (key == nullptr) {
    return nullptr;
  }

  for (extension_data& extension : extensions_) {
    if (extension.key == key) {
      return &extension;
    }
  }

  return nullptr;
}

error type_data::add_extension(extension_data extension) {
  if (!extension.valid()) {
    return {error_domain::meta, errc::invalid_argument};
  }

  if (contains_extension(extension.key)) {
    return {error_domain::meta, errc::duplicate_component};
  }

  extensions_.push_back(std::move(extension));
  return {};
}

} // namespace caors::meta::detail
