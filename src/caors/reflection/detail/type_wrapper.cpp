// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/reflection/detail/type_wrapper.h"

namespace caors::reflection::detail {

type_wrapper::type_wrapper(type_ops ops) noexcept : ops_(ops) {}

bool type_wrapper::valid() const noexcept {
  if (!ops_.valid() || !get_type()) {
    return false;
  }

  for (const field_wrapper& field : fields_) {
    if (!field.valid()) {
      return false;
    }
  }

  for (const method_wrapper& method : methods_) {
    if (!method.valid()) {
      return false;
    }
  }

  return true;
}

meta::type type_wrapper::get_type() const noexcept {
  return ops_.get_type != nullptr ? ops_.get_type() : meta::type{};
}

std::size_t type_wrapper::field_count() const noexcept { return fields_.size(); }

const field_wrapper* type_wrapper::field_at(std::size_t index) const noexcept {
  return index < fields_.size() ? &fields_[index] : nullptr;
}

field_wrapper* type_wrapper::mutable_field_at(std::size_t index) noexcept {
  return index < fields_.size() ? &fields_[index] : nullptr;
}

const field_wrapper* type_wrapper::find_field_ptr(core::string_name name) const noexcept {
  if (!name) {
    return nullptr;
  }

  for (const field_wrapper& field : fields_) {
    if (field.name() == name) {
      return &field;
    }
  }

  return nullptr;
}

std::optional<field_wrapper> type_wrapper::find_field(core::string_name name) const {
  const field_wrapper* field = find_field_ptr(name);
  if (field != nullptr) {
    return *field;
  }

  return std::nullopt;
}

std::optional<field_wrapper> type_wrapper::find_field(std::string_view name) const {
  const std::optional<core::string_name> interned_name = core::string_name::find(name);
  if (!interned_name.has_value()) {
    return std::nullopt;
  }

  return find_field(*interned_name);
}

void type_wrapper::add_field(field_wrapper field) { fields_.push_back(field); }

std::size_t type_wrapper::method_count() const noexcept { return methods_.size(); }

const method_wrapper* type_wrapper::method_at(std::size_t index) const noexcept {
  return index < methods_.size() ? &methods_[index] : nullptr;
}

const method_wrapper* type_wrapper::find_method_ptr(core::string_name name) const noexcept {
  if (!name) {
    return nullptr;
  }

  for (const method_wrapper& method : methods_) {
    if (method.name() == name) {
      return &method;
    }
  }

  return nullptr;
}

std::optional<method_wrapper> type_wrapper::find_method(core::string_name name) const {
  const method_wrapper* method = find_method_ptr(name);
  if (method != nullptr) {
    return *method;
  }

  return std::nullopt;
}

std::optional<method_wrapper> type_wrapper::find_method(std::string_view name) const {
  const std::optional<core::string_name> interned_name = core::string_name::find(name);
  if (!interned_name.has_value()) {
    return std::nullopt;
  }

  return find_method(*interned_name);
}

void type_wrapper::add_method(method_wrapper method) { methods_.push_back(method); }

} // namespace caors::reflection::detail
