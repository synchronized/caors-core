// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/reflection.h"

#include "caors/object/ptr_view.h"
#include "caors/reflection/detail/field_data.h"

namespace caors::reflection {
namespace {

[[nodiscard]] meta::type field_meta(const detail::field_data* field) {
  if (field == nullptr || !field->type) {
    return {};
  }

  return meta::detail::make_type_view(field->type, field->descriptor);
}

[[nodiscard]] const meta::detail::type_descriptor*
field_descriptor(const detail::field_data* field) noexcept {
  return field != nullptr && field->type ? field->descriptor : nullptr;
}

[[nodiscard]] type_id field_value_type(const meta::detail::type_descriptor* descriptor) noexcept {
  return descriptor != nullptr ? meta::detail::descriptor_value_type(*descriptor) : type_id{};
}

[[nodiscard]] type_id field_key_type(const meta::detail::type_descriptor* descriptor) noexcept {
  return descriptor != nullptr ? meta::detail::descriptor_key_type(*descriptor) : type_id{};
}

[[nodiscard]] const meta::detail::type_descriptor*
field_value_descriptor(const meta::detail::type_descriptor* descriptor) noexcept {
  if (descriptor == nullptr) {
    return nullptr;
  }

  switch (meta::detail::descriptor_kind(*descriptor)) {
  case meta::type_kind::raw_pointer:
  case meta::type_kind::wrapper: {
    const auto* indirect =
        meta::detail::descriptor_capability<meta::detail::indirect_data>(descriptor);
    return indirect != nullptr ? indirect->value_descriptor : nullptr;
  }
  case meta::type_kind::sequence: {
    const auto* sequence =
        meta::detail::descriptor_capability<meta::detail::sequence_data>(descriptor);
    return sequence != nullptr ? sequence->value_descriptor : nullptr;
  }
  case meta::type_kind::set: {
    const auto* set = meta::detail::descriptor_capability<meta::detail::set_data>(descriptor);
    return set != nullptr ? set->value_descriptor : nullptr;
  }
  case meta::type_kind::associative: {
    const auto* associative =
        meta::detail::descriptor_capability<meta::detail::associative_data>(descriptor);
    return associative != nullptr ? associative->value_descriptor : nullptr;
  }
  case meta::type_kind::pair: {
    const auto* pair = meta::detail::descriptor_capability<meta::detail::pair_data>(descriptor);
    return pair != nullptr ? pair->value_descriptor : nullptr;
  }
  case meta::type_kind::value:
  case meta::type_kind::enumeration:
    return descriptor;
  case meta::type_kind::tuple:
  case meta::type_kind::variant:
    return nullptr;
  }

  return nullptr;
}

[[nodiscard]] const meta::detail::type_descriptor*
field_key_descriptor(const meta::detail::type_descriptor* descriptor) noexcept {
  if (descriptor == nullptr) {
    return nullptr;
  }

  switch (meta::detail::descriptor_kind(*descriptor)) {
  case meta::type_kind::associative: {
    const auto* associative =
        meta::detail::descriptor_capability<meta::detail::associative_data>(descriptor);
    return associative != nullptr ? associative->key_descriptor : nullptr;
  }
  case meta::type_kind::pair: {
    const auto* pair = meta::detail::descriptor_capability<meta::detail::pair_data>(descriptor);
    return pair != nullptr ? pair->key_descriptor : nullptr;
  }
  case meta::type_kind::value:
  case meta::type_kind::enumeration:
  case meta::type_kind::raw_pointer:
  case meta::type_kind::wrapper:
  case meta::type_kind::sequence:
  case meta::type_kind::set:
  case meta::type_kind::tuple:
  case meta::type_kind::variant:
    return nullptr;
  }

  return nullptr;
}

[[nodiscard]] core::string_name
type_name_of(type_id id, const meta::detail::type_descriptor* descriptor) noexcept {
  const meta::type type = meta::detail::make_type_view(id, descriptor);
  return type ? type.name() : core::string_name{};
}

} // namespace

field::field(meta::type owner, const detail::field_data* field) noexcept
    : owner_(owner), field_(field) {}

bool field::valid() const noexcept {
  return owner_.valid() && field_ != nullptr && field_->valid();
}

core::string_name field::name() const noexcept {
  const detail::field_data* field = info();
  return field != nullptr ? field->name : core::string_name{};
}

type_id field::type() const noexcept {
  const detail::field_data* field = info();
  return field != nullptr ? field->type : type_id{};
}

meta::type field::get_type() const noexcept { return field_meta(info()); }

core::string_name field::type_name() const noexcept {
  const meta::type type = get_type();
  return type ? type.name() : core::string_name{};
}

type_id field::value_type() const noexcept {
  const meta::detail::type_descriptor* descriptor = field_descriptor(info());
  return field_value_type(descriptor);
}

meta::type field::get_value_type() const noexcept {
  const meta::detail::type_descriptor* descriptor = field_descriptor(info());
  return meta::detail::make_type_view(field_value_type(descriptor),
                                      field_value_descriptor(descriptor));
}

type_id field::element_type() const noexcept {
  const meta::detail::type_descriptor* descriptor = field_descriptor(info());
  return field_value_type(descriptor);
}

meta::type field::get_element_type() const noexcept { return get_value_type(); }

core::string_name field::element_type_name() const noexcept {
  const meta::detail::type_descriptor* descriptor = field_descriptor(info());
  return type_name_of(element_type(), field_value_descriptor(descriptor));
}

type_id field::key_type() const noexcept {
  const meta::detail::type_descriptor* descriptor = field_descriptor(info());
  return field_key_type(descriptor);
}

meta::type field::get_key_type() const noexcept {
  const meta::detail::type_descriptor* descriptor = field_descriptor(info());
  return meta::detail::make_type_view(field_key_type(descriptor), field_key_descriptor(descriptor));
}

core::string_name field::key_type_name() const noexcept {
  const meta::detail::type_descriptor* descriptor = field_descriptor(info());
  return type_name_of(key_type(), field_key_descriptor(descriptor));
}

type_id field::first_type() const noexcept { return key_type(); }

meta::type field::get_first_type() const noexcept { return get_key_type(); }

core::string_name field::first_type_name() const noexcept {
  const meta::detail::type_descriptor* descriptor = field_descriptor(info());
  return type_name_of(first_type(), field_key_descriptor(descriptor));
}

type_id field::mapped_type() const noexcept {
  const meta::detail::type_descriptor* descriptor = field_descriptor(info());
  return field_value_type(descriptor);
}

meta::type field::get_mapped_type() const noexcept { return get_value_type(); }

core::string_name field::mapped_type_name() const noexcept {
  const meta::detail::type_descriptor* descriptor = field_descriptor(info());
  return type_name_of(mapped_type(), field_value_descriptor(descriptor));
}

type_id field::second_type() const noexcept { return value_type(); }

meta::type field::get_second_type() const noexcept { return get_value_type(); }

core::string_name field::second_type_name() const noexcept {
  const meta::detail::type_descriptor* descriptor = field_descriptor(info());
  return type_name_of(second_type(), field_value_descriptor(descriptor));
}

meta::type_kind field::kind() const noexcept {
  const meta::detail::type_descriptor* descriptor = field_descriptor(info());
  return descriptor != nullptr ? meta::detail::descriptor_kind(*descriptor)
                               : meta::type_kind::value;
}

meta::wrapper_semantics field::wrapper_semantics() const noexcept {
  const meta::detail::type_descriptor* descriptor = field_descriptor(info());
  return descriptor != nullptr ? meta::detail::descriptor_wrapper_semantics(*descriptor)
                               : meta::wrapper_semantics::none;
}

bool field::indirect() const noexcept {
  const meta::detail::type_descriptor* descriptor = field_descriptor(info());
  return descriptor != nullptr && meta::detail::descriptor_indirect(*descriptor);
}

bool field::nullable() const noexcept {
  const meta::detail::type_descriptor* descriptor = field_descriptor(info());
  return descriptor != nullptr && meta::detail::descriptor_nullable(*descriptor);
}

bool field::writable() const noexcept {
  const detail::field_data* field = info();
  return field != nullptr && field->writable;
}

std::size_t field::offset() const noexcept {
  const detail::field_data* field = info();
  return field != nullptr ? field->offset : 0;
}

std::size_t field::size() const noexcept {
  const detail::field_data* field = info();
  if (field != nullptr && field->descriptor != nullptr) {
    return field->descriptor->layout.size;
  }

  const meta::type meta = field_meta(info());
  return meta ? meta.size() : 0;
}

std::size_t field::alignment() const noexcept {
  const detail::field_data* field = info();
  if (field != nullptr && field->descriptor != nullptr) {
    return field->descriptor->layout.alignment;
  }

  const meta::type meta = field_meta(info());
  return meta ? meta.alignment() : 0;
}

const detail::field_data* field::info() const noexcept {
  if (!valid()) {
    return nullptr;
  }

  return field_;
}

any field::get_value(const void* object) const {
  const detail::field_data* field = info();
  if (field == nullptr) {
    return {};
  }

  return field->get_value(object);
}

any_ref field::get_ref(const void* object) const {
  const detail::field_data* field = info();
  if (field == nullptr) {
    return {};
  }

  return field->get_ref(object);
}

any_ref field::get_ref(void* object) const {
  const detail::field_data* field = info();
  if (field == nullptr) {
    return {};
  }

  return field->get_ref(object);
}

bool field::try_set_value(void* object, const any& source) const {
  const detail::field_data* field = info();
  if (field == nullptr) {
    return false;
  }

  return field->try_set_value(object, source);
}

objects::object* field::get_object_ptr(const void* object) const {
  const any_ref ref = get_ref(object);
  const objects::ptr_view object_ptr = objects::ptr_view_of(ref);
  if (!object_ptr) {
    return nullptr;
  }

  return object_ptr.get();
}

bool field::try_set_object_ptr(void* object, objects::object* instance) const {
  const any_ref ref = get_ref(object);
  const objects::ptr_view object_ptr = objects::ptr_view_of(ref);
  if (!object_ptr) {
    return false;
  }

  return object_ptr.set(instance);
}

bool field::is_sequence() const noexcept { return kind() == meta::type_kind::sequence; }

bool field::is_set() const noexcept { return kind() == meta::type_kind::set; }

bool field::is_associative() const noexcept { return kind() == meta::type_kind::associative; }

bool field::is_pair() const noexcept { return kind() == meta::type_kind::pair; }

bool field::is_tuple() const noexcept { return kind() == meta::type_kind::tuple; }

bool field::is_variant() const noexcept { return kind() == meta::type_kind::variant; }

} // namespace caors::reflection
