// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/reflection.h"

#include "caors/reflection/detail/field_wrapper.h"

namespace caors::reflection {
namespace {

[[nodiscard]] const detail::field_wrapper* as_field_wrapper(const void* field) noexcept {
  return static_cast<const detail::field_wrapper*>(field);
}

} // namespace

field::field(const void* field) noexcept : field_(field) {}

bool field::valid() const noexcept {
  const detail::field_wrapper* field = as_field_wrapper(field_);
  return field != nullptr && field->valid();
}

core::string_name field::name() const noexcept {
  const detail::field_wrapper* field = as_field_wrapper(info());
  return field != nullptr ? field->name() : core::string_name{};
}

meta::type field::get_owner_type() const noexcept {
  const detail::field_wrapper* field = as_field_wrapper(info());
  return field != nullptr ? field->get_owner_type() : meta::type{};
}

meta::type field::get_type() const noexcept {
  const detail::field_wrapper* field = as_field_wrapper(info());
  return field != nullptr ? field->get_type() : meta::type{};
}

meta::type field::get_value_type() const noexcept { return get_type().get_value_type(); }

meta::type field::get_element_type() const noexcept { return get_value_type(); }

meta::type field::get_key_type() const noexcept { return get_type().get_key_type(); }

meta::type field::get_first_type() const noexcept { return get_key_type(); }

meta::type field::get_mapped_type() const noexcept { return get_value_type(); }

meta::type field::get_second_type() const noexcept { return get_value_type(); }

meta::type_kind field::kind() const noexcept { return get_type().kind(); }

meta::wrapper_semantics field::wrapper_semantics() const noexcept { return get_type().wrapper(); }

bool field::indirect() const noexcept { return get_type().indirect(); }

bool field::nullable() const noexcept { return get_type().nullable(); }

bool field::writable() const noexcept {
  const detail::field_wrapper* field = as_field_wrapper(info());
  return field != nullptr && field->writable();
}

std::size_t field::offset() const noexcept {
  const detail::field_wrapper* field = as_field_wrapper(info());
  return field != nullptr ? field->offset() : 0;
}

std::size_t field::size() const noexcept {
  const meta::type meta = get_type();
  const meta::value_storage_ops* value_ops = meta.get_capability<meta::value_storage_ops>();
  return value_ops != nullptr ? value_ops->layout.size : 0;
}

std::size_t field::alignment() const noexcept {
  const meta::type meta = get_type();
  const meta::value_storage_ops* value_ops = meta.get_capability<meta::value_storage_ops>();
  return value_ops != nullptr ? value_ops->layout.alignment : 0;
}

const void* field::info() const noexcept {
  if (!valid()) {
    return nullptr;
  }

  return field_;
}

any field::get_value(const void* object) const {
  const detail::field_wrapper* field = as_field_wrapper(info());
  if (field == nullptr) {
    return {};
  }

  return field->get_value(object);
}

any_ref field::get_ref(const void* object) const {
  const detail::field_wrapper* field = as_field_wrapper(info());
  if (field == nullptr) {
    return {};
  }

  return field->get_ref(object);
}

any_ref field::get_ref(void* object) const {
  const detail::field_wrapper* field = as_field_wrapper(info());
  if (field == nullptr) {
    return {};
  }

  return field->get_ref(object);
}

bool field::try_set_value(void* object, const any& source) const {
  const detail::field_wrapper* field = as_field_wrapper(info());
  if (field == nullptr) {
    return false;
  }

  return field->try_set_value(object, source);
}

bool field::is_sequence() const noexcept { return kind() == meta::type_kind::sequence; }

bool field::is_set() const noexcept { return kind() == meta::type_kind::set; }

bool field::is_associative() const noexcept { return kind() == meta::type_kind::associative; }

bool field::is_pair() const noexcept { return kind() == meta::type_kind::pair; }

bool field::is_tuple() const noexcept { return kind() == meta::type_kind::tuple; }

bool field::is_variant() const noexcept { return kind() == meta::type_kind::variant; }

} // namespace caors::reflection
