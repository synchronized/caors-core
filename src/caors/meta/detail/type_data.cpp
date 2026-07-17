// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta/detail/type_data.h"

namespace caors::meta::detail {

bool type_data::reflection_valid() const noexcept {
  if (!reflection_ops_.valid() || !get_type()) {
    return false;
  }

  for (const field_data& field : fields_) {
    if (!field.valid()) {
      return false;
    }
  }

  for (const method_data& method : methods_) {
    if (!method.valid()) {
      return false;
    }
  }

  return true;
}

meta::type type_data::get_type() const noexcept {
  return reflection_ops_.get_type != nullptr ? reflection_ops_.get_type() : meta::type{};
}

std::size_t type_data::field_count() const noexcept { return fields_.size(); }

const field_data* type_data::field_at(std::size_t index) const noexcept {
  return index < fields_.size() ? &fields_[index] : nullptr;
}

field_data* type_data::mutable_field_at(std::size_t index) noexcept {
  return index < fields_.size() ? &fields_[index] : nullptr;
}

const field_data* type_data::find_field_ptr(core::string_name name) const noexcept {
  if (!name) {
    return nullptr;
  }

  for (const field_data& field : fields_) {
    if (field.name() == name) {
      return &field;
    }
  }

  return nullptr;
}

field_data* type_data::mutable_find_field_ptr(core::string_name name) noexcept {
  if (!name) {
    return nullptr;
  }

  for (field_data& field : fields_) {
    if (field.name() == name) {
      return &field;
    }
  }

  return nullptr;
}

std::optional<field_data> type_data::find_field(core::string_name name) const {
  const field_data* field = find_field_ptr(name);
  if (field != nullptr) {
    return *field;
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

void type_data::add_field(field_data field) { fields_.push_back(field); }

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

error type_data::attach_reflection(const type_data& reflection) {
  if (has_reflection()) {
    return {error_domain::reflection, errc::duplicate_reflection};
  }

  if (!reflection.has_reflection()) {
    return {error_domain::reflection, errc::invalid_type};
  }

  reflection_ops_ = reflection.reflection_ops_;
  fields_ = reflection.fields_;
  methods_ = reflection.methods_;
  return {};
}

type_data* type_data::ensure_reflection(reflection_type_ops ops) noexcept {
  if (!ops.valid()) {
    return nullptr;
  }

  if (!has_reflection()) {
    reflection_ops_ = ops;
  }

  return this;
}

} // namespace caors::meta::detail
