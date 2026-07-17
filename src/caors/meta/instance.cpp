// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta/instance.h"

namespace caors::meta {

instance::instance(meta::any_ref value) noexcept : value_(value) {}

bool instance::valid() const noexcept { return value_.valid(); }

meta::any_ref instance::value() const noexcept { return value_; }

meta::type instance::type() const noexcept { return valid() ? value_.get_type() : meta::type{}; }

const void* instance::data() const noexcept { return value_.data(); }

void* instance::mutable_data() const noexcept { return value_.mutable_data(); }

bool instance::writable() const noexcept { return value_.writable(); }

std::size_t instance::property_count() const noexcept { return type().property_count(); }

property instance::property_at(std::size_t index) const { return type().property_at(index); }

property instance::find_property(string_name name) const { return type().find_property(name); }

property instance::find_property(std::string_view name) const { return type().find_property(name); }

core::vector<property> instance::properties(memory_resource* resource) const {
  core::vector<property> result{allocator<property>{resource}};
  const std::size_t count = property_count();
  result.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    result.push_back(property_at(index));
  }

  return result;
}

std::size_t instance::method_count() const noexcept { return type().method_count(); }

method_set instance::method_at(std::size_t index) const { return type().method_at(index); }

method_set instance::find_method(string_name name) const { return type().find_method(name); }

method_set instance::find_method(std::string_view name) const { return type().find_method(name); }

core::vector<method_set> instance::methods(memory_resource* resource) const {
  core::vector<method_set> result{allocator<method_set>{resource}};
  const std::size_t count = method_count();
  result.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    result.push_back(method_at(index));
  }

  return result;
}

any instance::get(std::string_view name) const {
  const property reflected_property = find_property(name);
  if (!reflected_property || !value_) {
    return {};
  }

  return reflected_property.get_value(*this);
}

any_ref instance::get_ref(std::string_view name) const {
  const property reflected_property = find_property(name);
  if (!reflected_property || !value_) {
    return {};
  }

  return reflected_property.get_ref(*this);
}

bool instance::set(std::string_view name, const any& source) const {
  const property reflected_property = find_property(name);
  if (!reflected_property || !value_.writable()) {
    return false;
  }

  return reflected_property.try_set_value(*this, source);
}

bool instance::set(std::string_view name, const any_ref& source) const {
  const any_ref target = get_ref(name);
  return target.try_set(source);
}

any instance::call(std::string_view name, std::span<const any_ref> arguments) const {
  const method_set reflected_method = find_method(name);
  if (!reflected_method || !value_) {
    return {};
  }

  return reflected_method.invoke(*this, arguments);
}

} // namespace caors::meta
