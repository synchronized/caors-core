// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/reflection.h"

#include "caors/reflection/detail/type_wrapper.h"

namespace caors::reflection {
namespace {

[[nodiscard]] const detail::type_wrapper* as_type_wrapper(const void* data) noexcept {
  return static_cast<const detail::type_wrapper*>(data);
}

} // namespace

type::type(meta::type type, const void* data) noexcept : type_(type), data_(data) {}

bool type::valid() const noexcept {
  const detail::type_wrapper* reflected = as_type_wrapper(data_);
  if (!type_.valid() || reflected == nullptr || !reflected->reflection_valid()) {
    return false;
  }

  const meta::type reflected_type = reflected->get_type();
  return reflected_type && reflected_type.id() == type_.id();
}

meta::type type::info() const noexcept { return valid() ? type_ : meta::type{}; }

type_id type::id() const noexcept { return type_.id(); }

core::string_name type::name() const noexcept { return type_.name(); }

core::type_guid type::guid() const noexcept { return type_.guid(); }

std::size_t type::field_count() const noexcept {
  const detail::type_wrapper* reflected = as_type_wrapper(data());
  return reflected != nullptr ? reflected->field_count() : 0;
}

field type::field_at(std::size_t index) const {
  const detail::type_wrapper* reflected = as_type_wrapper(data());
  if (reflected == nullptr) {
    return {};
  }

  return field{reflected->field_at(index)};
}

field type::find_field(core::string_name name) const {
  const detail::type_wrapper* reflected = as_type_wrapper(data());
  if (reflected == nullptr || !name) {
    return {};
  }

  return field{reflected->find_field_ptr(name)};
}

field type::find_field(std::string_view name) const {
  const std::optional<core::string_name> interned_name = core::string_name::find(name);
  if (!interned_name.has_value()) {
    return {};
  }

  return find_field(*interned_name);
}

std::size_t type::method_count() const noexcept {
  const detail::type_wrapper* reflected = as_type_wrapper(data());
  return reflected != nullptr ? reflected->method_count() : 0;
}

method type::method_at(std::size_t index) const {
  const detail::type_wrapper* reflected = as_type_wrapper(data());
  if (reflected == nullptr) {
    return {};
  }

  return method{reflected->method_at(index)};
}

method type::find_method(core::string_name name) const {
  const detail::type_wrapper* reflected = as_type_wrapper(data());
  if (reflected == nullptr || !name) {
    return {};
  }

  return method{reflected->find_method_ptr(name)};
}

method type::find_method(std::string_view name) const {
  const std::optional<core::string_name> interned_name = core::string_name::find(name);
  if (!interned_name.has_value()) {
    return {};
  }

  return find_method(*interned_name);
}

const void* type::data() const noexcept {
  if (!valid()) {
    return nullptr;
  }

  return data_;
}

} // namespace caors::reflection
