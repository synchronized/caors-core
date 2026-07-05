// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/reflection.h"

#include "caors/reflection/detail/type_data.h"

namespace caors::reflection {

type::type(meta::type type, const detail::type_data* data) noexcept : type_(type), data_(data) {}

bool type::valid() const noexcept { return type_.valid() && data_ != nullptr && data_->valid(); }

meta::type type::info() const noexcept { return valid() ? type_ : meta::type{}; }

type_id type::id() const noexcept { return type_.id(); }

core::string_name type::name() const noexcept { return type_.name(); }

core::type_guid type::guid() const noexcept { return type_.guid(); }

std::size_t type::field_count() const noexcept {
  const detail::type_data* reflected = data();
  return reflected != nullptr ? reflected->fields.size() : 0;
}

field type::field_at(std::size_t index) const {
  const detail::type_data* reflected = data();
  if (reflected == nullptr || index >= reflected->fields.size()) {
    return {};
  }

  return field{type_, &reflected->fields[index]};
}

field type::find_field(core::string_name name) const {
  const detail::type_data* reflected = data();
  if (reflected == nullptr || !name) {
    return {};
  }

  for (std::size_t index = 0; index < reflected->fields.size(); ++index) {
    if (reflected->fields[index].name == name) {
      return field_at(index);
    }
  }

  return {};
}

field type::find_field(std::string_view name) const {
  const std::optional<core::string_name> interned_name = core::string_name::find(name);
  if (!interned_name.has_value()) {
    return {};
  }

  return find_field(*interned_name);
}

std::size_t type::method_count() const noexcept {
  const detail::type_data* reflected = data();
  return reflected != nullptr ? reflected->methods.size() : 0;
}

method type::method_at(std::size_t index) const {
  const detail::type_data* reflected = data();
  if (reflected == nullptr || index >= reflected->methods.size()) {
    return {};
  }

  return method{type_, &reflected->methods[index]};
}

method type::find_method(core::string_name name) const {
  const detail::type_data* reflected = data();
  if (reflected == nullptr || !name) {
    return {};
  }

  for (std::size_t index = 0; index < reflected->methods.size(); ++index) {
    if (reflected->methods[index].name == name) {
      return method_at(index);
    }
  }

  return {};
}

method type::find_method(std::string_view name) const {
  const std::optional<core::string_name> interned_name = core::string_name::find(name);
  if (!interned_name.has_value()) {
    return {};
  }

  return find_method(*interned_name);
}

const detail::type_data* type::data() const noexcept {
  if (!valid()) {
    return nullptr;
  }

  return data_;
}

} // namespace caors::reflection
