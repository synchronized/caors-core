// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/reflection.h"

namespace caors::reflection {
namespace {

[[nodiscard]] bool same_type_data(meta::type lhs, const meta::detail::type_data& rhs) noexcept {
  return lhs.id() == rhs.id() && lhs.name() == rhs.name() && lhs.guid() == rhs.guid() &&
         lhs.parent_id() == rhs.parent_id();
}

[[nodiscard]] bool resolve_field_type(meta::registry& meta, detail::field_data& field) {
  if (!field.valid()) {
    return false;
  }

  if (field.type && meta.contains(field.type)) {
    return true;
  }

  return field.descriptor != nullptr && field.descriptor->has_view_capability();
}

[[nodiscard]] error resolve_fields(meta::registry& meta, detail::type_data& reflection) {
  for (detail::field_data& field : reflection.fields) {
    if (!resolve_field_type(meta, field)) {
      return {error_domain::reflection, errc::invalid_field};
    }
  }

  return {};
}

[[nodiscard]] error validate_registered_fields(const detail::type_data& reflection) {
  core::hash_set<core::string_name> names;
  names.reserve(reflection.fields.size());

  for (const detail::field_data& field : reflection.fields) {
    if (!field.valid() || !field.type) {
      return {error_domain::reflection, errc::invalid_field};
    }

    if (!names.insert(field.name).second) {
      return {error_domain::reflection, errc::duplicate_field};
    }
  }

  return {};
}

[[nodiscard]] meta::type field_meta(const detail::field_data* field) {
  if (field == nullptr || !field->type) {
    return {};
  }

  return caors::meta::find(field->type);
}

[[nodiscard]] const meta::detail::type_descriptor*
field_descriptor(const detail::field_data* field) noexcept {
  return field != nullptr && field->type ? field->descriptor : nullptr;
}

[[nodiscard]] core::string_name type_name_of(type_id id) noexcept {
  const meta::type type = caors::meta::find(id);
  return type ? type.name() : core::string_name{};
}

} // namespace

bool detail::type_data::valid() const noexcept {
  for (const detail::field_data& field : fields) {
    if (!field.valid()) {
      return false;
    }
  }

  return true;
}

std::optional<detail::field_data> detail::type_data::find_field(core::string_name name) const {
  if (!name) {
    return std::nullopt;
  }

  for (const detail::field_data& field : fields) {
    if (field.name == name) {
      return field;
    }
  }

  return std::nullopt;
}

std::optional<detail::field_data> detail::type_data::find_field(std::string_view name) const {
  const std::optional<core::string_name> interned_name = core::string_name::find(name);
  if (!interned_name.has_value()) {
    return std::nullopt;
  }

  return find_field(*interned_name);
}

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

core::string_name field::type_name() const noexcept {
  const meta::type meta = field_meta(info());
  return meta ? meta.name() : core::string_name{};
}

type_id field::value_type() const noexcept {
  const meta::detail::type_descriptor* descriptor = field_descriptor(info());
  return descriptor != nullptr ? descriptor->value_type : type_id{};
}

type_id field::element_type() const noexcept {
  const meta::detail::type_descriptor* descriptor = field_descriptor(info());
  return descriptor != nullptr ? descriptor->value_type : type_id{};
}

core::string_name field::element_type_name() const noexcept { return type_name_of(element_type()); }

type_id field::key_type() const noexcept {
  const meta::detail::type_descriptor* descriptor = field_descriptor(info());
  return descriptor != nullptr ? descriptor->key_type : type_id{};
}

core::string_name field::key_type_name() const noexcept { return type_name_of(key_type()); }

type_id field::first_type() const noexcept { return key_type(); }

core::string_name field::first_type_name() const noexcept { return type_name_of(first_type()); }

type_id field::mapped_type() const noexcept {
  const meta::detail::type_descriptor* descriptor = field_descriptor(info());
  return descriptor != nullptr ? descriptor->value_type : type_id{};
}

core::string_name field::mapped_type_name() const noexcept { return type_name_of(mapped_type()); }

type_id field::second_type() const noexcept { return value_type(); }

core::string_name field::second_type_name() const noexcept { return type_name_of(second_type()); }

meta::type_kind field::kind() const noexcept {
  const meta::detail::type_descriptor* descriptor = field_descriptor(info());
  return descriptor != nullptr ? descriptor->kind : meta::type_kind::value;
}

meta::wrapper_semantics field::wrapper_semantics() const noexcept {
  const meta::detail::type_descriptor* descriptor = field_descriptor(info());
  return descriptor != nullptr ? descriptor->wrapper_semantics : meta::wrapper_semantics::none;
}

std::uint8_t field::pointer_depth() const noexcept {
  const meta::detail::type_descriptor* descriptor = field_descriptor(info());
  return descriptor != nullptr ? descriptor->pointer_depth : 0;
}

bool field::nullable() const noexcept {
  const meta::detail::type_descriptor* descriptor = field_descriptor(info());
  return descriptor != nullptr && descriptor->nullable;
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
    return field->descriptor->size;
  }

  const meta::type meta = field_meta(info());
  return meta ? meta.size() : 0;
}

std::size_t field::alignment() const noexcept {
  const detail::field_data* field = info();
  if (field != nullptr && field->descriptor != nullptr) {
    return field->descriptor->alignment;
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

object* field::get_object_ref(const void* object) const {
  const detail::field_data* field = info();
  if (field == nullptr || field->get_object_ref == nullptr) {
    return nullptr;
  }

  return field->get_object_ref(field->data(object));
}

bool field::try_set_object_ref(void* object, caors::object* instance) const {
  const detail::field_data* field = info();
  if (field == nullptr || field->set_object_ref == nullptr) {
    return false;
  }

  return field->set_object_ref(field->mutable_data(object), instance);
}

bool field::is_sequence() const noexcept { return kind() == meta::type_kind::sequence; }

bool field::is_set() const noexcept { return kind() == meta::type_kind::set; }

bool field::is_associative() const noexcept { return kind() == meta::type_kind::associative; }

bool field::is_pair() const noexcept { return kind() == meta::type_kind::pair; }

bool field::is_tuple() const noexcept { return kind() == meta::type_kind::tuple; }

bool field::is_variant() const noexcept { return kind() == meta::type_kind::variant; }

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

const detail::type_data* type::data() const noexcept {
  if (!valid()) {
    return nullptr;
  }

  return data_;
}

registry::registry() = default;

registry::~registry() = default;

registry& registry::instance() noexcept {
  static registry registry;
  return registry;
}

error registry::register_type(const meta::detail::type_data& type,
                              const detail::type_data& reflection) {
  if (!type.valid() || !type.id()) {
    return {error_domain::reflection, errc::invalid_type};
  }

  if (!reflection.valid()) {
    return {error_domain::reflection, errc::invalid_field};
  }

  meta::type existing;
  if (type.id()) {
    existing = meta::get_registry().find_type(type.id());
  }
  if (!existing) {
    existing = meta::get_registry().find_type(type.name());
  }
  if (!existing && type.guid()) {
    existing = meta::get_registry().find_type(type.guid());
  }

  if (existing) {
    if (!same_type_data(existing, type)) {
      return {error_domain::reflection, errc::inconsistent_type};
    }
  }

  const error type_status = meta::get_registry().register_type(type);
  if (type_status && type_status.value() != errc::duplicate_id) {
    return rethrow_as(type_status, error_domain::reflection);
  }

  const meta::type stored_meta = meta::get_registry().find_type(type.id());
  if (!stored_meta) {
    return {error_domain::reflection, errc::invalid_type};
  }

  detail::type_data stored_reflection = reflection;

  const error field_status = resolve_fields(meta::get_registry(), stored_reflection);
  if (field_status) {
    return field_status;
  }

  const error validation_status = validate_registered_fields(stored_reflection);
  if (validation_status) {
    return validation_status;
  }

  return rethrow_as(
      meta::get_registry().emplace<detail::type_data>(stored_meta.id(), stored_reflection),
      error_domain::reflection, errc::duplicate_component, errc::duplicate_reflection);
}

error registry::register_type(type_id id, const detail::type_data& reflection) {
  if (!id) {
    return {error_domain::reflection, errc::invalid_type};
  }

  const meta::type stored_meta = meta::get_registry().find_type(id);
  if (!stored_meta) {
    return {error_domain::reflection, errc::missing_type};
  }

  if (!reflection.valid()) {
    return {error_domain::reflection, errc::invalid_field};
  }

  detail::type_data stored_reflection = reflection;

  const error field_status = resolve_fields(meta::get_registry(), stored_reflection);
  if (field_status) {
    return field_status;
  }

  const error validation_status = validate_registered_fields(stored_reflection);
  if (validation_status) {
    return validation_status;
  }

  return rethrow_as(meta::get_registry().emplace<detail::type_data>(id, stored_reflection),
                    error_domain::reflection, errc::duplicate_component,
                    errc::duplicate_reflection);
}

error registry::register_type(meta::type type, const detail::type_data& reflection) {
  if (!type.valid()) {
    return {error_domain::reflection, errc::invalid_type};
  }

  return register_type(type.id(), reflection);
}

bool registry::contains(type_id id) const {
  return meta::get_registry().has<detail::type_data>(id);
}

const detail::type_data* registry::find_reflection_data(type_id id) const {
  return meta::get_registry().find_ptr<detail::type_data>(id);
}

type registry::find_type(type_id id) const {
  const meta::type meta_type = meta::get_registry().find_type(id);
  if (!meta_type) {
    return {};
  }

  const detail::type_data* reflection = find_reflection_data(id);
  if (reflection == nullptr) {
    return {};
  }

  return type{meta_type, reflection};
}

type registry::find_type(core::type_guid guid) const {
  const meta::type type = meta::get_registry().find_type(guid);
  if (!type) {
    return {};
  }

  return find_type(type.id());
}

type registry::find_type(std::string_view name) const {
  const meta::type type = meta::get_registry().find_type(name);
  if (!type) {
    return {};
  }

  return find_type(type.id());
}

type registry::find_type(core::string_name name) const {
  const meta::type type = meta::get_registry().find_type(name);
  if (!type) {
    return {};
  }

  return find_type(type.id());
}

std::size_t registry::size() const noexcept {
  return meta::get_registry().count<detail::type_data>();
}

registry& get_registry() noexcept { return registry::instance(); }

type find(type_id id) { return get_registry().find_type(id); }

type find(core::type_guid guid) { return get_registry().find_type(guid); }

type find(std::string_view name) { return get_registry().find_type(name); }

bool contains(type_id id) { return get_registry().contains(id); }

std::size_t size() noexcept { return get_registry().size(); }

} // namespace caors::reflection
