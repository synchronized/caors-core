// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/reflection.h"

namespace caors {
namespace {

[[nodiscard]] bool same_type_data(meta_type lhs, const meta_type_data& rhs) noexcept {
  return lhs.name() == rhs.name() && lhs.guid() == rhs.guid() && lhs.size() == rhs.size() &&
         lhs.alignment() == rhs.alignment() && lhs.parent_id() == rhs.parent_id();
}

[[nodiscard]] bool resolve_field_type(meta_registry& meta, reflection_field_data& field) {
  if (!field.valid()) {
    return false;
  }

  if (field.type && meta.contains(field.type)) {
    return true;
  }

  return false;
}

[[nodiscard]] error resolve_fields(meta_registry& meta, reflection_type_data& reflection) {
  for (reflection_field_data& field : reflection.fields) {
    if (!resolve_field_type(meta, field)) {
      return {error_domain::reflection, errc::invalid_field};
    }
  }

  return {};
}

[[nodiscard]] error validate_registered_fields(const reflection_type_data& reflection) {
  hash_set<string_name> names;
  names.reserve(reflection.fields.size());

  for (const reflection_field_data& field : reflection.fields) {
    if (!field.valid() || !field.type) {
      return {error_domain::reflection, errc::invalid_field};
    }

    if (!names.insert(field.name).second) {
      return {error_domain::reflection, errc::duplicate_field};
    }
  }

  return {};
}

[[nodiscard]] std::optional<meta_type> field_meta(const reflection_field_data* field) {
  if (field == nullptr || !field->type) {
    return std::nullopt;
  }

  return caors::find_type(field->type);
}

[[nodiscard]] const meta_type_semantics_data*
field_type_semantics(const reflection_field_data* field) noexcept {
  if (field == nullptr || !field->type) {
    return nullptr;
  }

  return get_meta_registry().find_ptr<meta_type_semantics_data>(field->type);
}

[[nodiscard]] string_name type_name_of(type_id id) noexcept {
  const std::optional<meta_type> type = caors::find_type(id);
  return type.has_value() ? type->name() : string_name{};
}

} // namespace

bool reflection_type_data::valid() const noexcept {
  for (const reflection_field_data& field : fields) {
    if (!field.valid()) {
      return false;
    }
  }

  return true;
}

std::optional<reflection_field_data> reflection_type_data::find_field(string_name name) const {
  if (!name) {
    return std::nullopt;
  }

  for (const reflection_field_data& field : fields) {
    if (field.name == name) {
      return field;
    }
  }

  return std::nullopt;
}

std::optional<reflection_field_data> reflection_type_data::find_field(std::string_view name) const {
  const std::optional<string_name> interned_name = string_name::find(name);
  if (!interned_name.has_value()) {
    return std::nullopt;
  }

  return find_field(*interned_name);
}

reflection_field::reflection_field(meta_type owner, const reflection_field_data* field) noexcept
    : owner_(owner), field_(field) {}

bool reflection_field::valid() const noexcept {
  return owner_.valid() && field_ != nullptr && field_->valid();
}

string_name reflection_field::name() const noexcept {
  const reflection_field_data* field = info();
  return field != nullptr ? field->name : string_name{};
}

type_id reflection_field::type() const noexcept {
  const reflection_field_data* field = info();
  return field != nullptr ? field->type : type_id{};
}

string_name reflection_field::type_name() const noexcept {
  const std::optional<meta_type> meta = field_meta(info());
  return meta.has_value() ? meta->name() : string_name{};
}

type_id reflection_field::value_type() const noexcept {
  const meta_type_semantics_data* traits = field_type_semantics(info());
  return traits != nullptr ? traits->value_type : type_id{};
}

type_id reflection_field::element_type() const noexcept {
  const meta_type_semantics_data* traits = field_type_semantics(info());
  return traits != nullptr ? traits->value_type : type_id{};
}

string_name reflection_field::element_type_name() const noexcept {
  return type_name_of(element_type());
}

type_id reflection_field::key_type() const noexcept {
  const meta_type_semantics_data* traits = field_type_semantics(info());
  return traits != nullptr ? traits->key_type : type_id{};
}

string_name reflection_field::key_type_name() const noexcept { return type_name_of(key_type()); }

type_id reflection_field::first_type() const noexcept { return key_type(); }

string_name reflection_field::first_type_name() const noexcept {
  return type_name_of(first_type());
}

type_id reflection_field::mapped_type() const noexcept {
  const meta_type_semantics_data* traits = field_type_semantics(info());
  return traits != nullptr ? traits->value_type : type_id{};
}

string_name reflection_field::mapped_type_name() const noexcept {
  return type_name_of(mapped_type());
}

type_id reflection_field::second_type() const noexcept { return value_type(); }

string_name reflection_field::second_type_name() const noexcept {
  return type_name_of(second_type());
}

type_kind reflection_field::kind() const noexcept {
  const meta_type_semantics_data* traits = field_type_semantics(info());
  return traits != nullptr ? traits->kind : type_kind::value;
}

wrapper_semantics reflection_field::wrapper_semantics() const noexcept {
  const meta_type_semantics_data* traits = field_type_semantics(info());
  return traits != nullptr ? traits->wrapper_semantics : wrapper_semantics::none;
}

std::uint8_t reflection_field::pointer_depth() const noexcept {
  const meta_type_semantics_data* traits = field_type_semantics(info());
  return traits != nullptr ? traits->pointer_depth : 0;
}

bool reflection_field::nullable() const noexcept {
  const meta_type_semantics_data* traits = field_type_semantics(info());
  return traits != nullptr && traits->nullable;
}

bool reflection_field::writable() const noexcept {
  const reflection_field_data* field = info();
  return field != nullptr && field->writable;
}

std::size_t reflection_field::offset() const noexcept {
  const reflection_field_data* field = info();
  return field != nullptr ? field->offset : 0;
}

std::size_t reflection_field::size() const noexcept {
  const std::optional<meta_type> meta = field_meta(info());
  return meta.has_value() ? meta->size() : 0;
}

std::size_t reflection_field::alignment() const noexcept {
  const std::optional<meta_type> meta = field_meta(info());
  return meta.has_value() ? meta->alignment() : 0;
}

const reflection_field_data* reflection_field::info() const noexcept {
  if (!valid()) {
    return nullptr;
  }

  return field_;
}

any reflection_field::get_value(const void* object) const {
  const reflection_field_data* field = info();
  if (field == nullptr) {
    return {};
  }

  return field->get_value(object);
}

any_ref reflection_field::get_ref(const void* object) const {
  const reflection_field_data* field = info();
  if (field == nullptr) {
    return {};
  }

  return field->get_ref(object);
}

any_ref reflection_field::get_ref(void* object) const {
  const reflection_field_data* field = info();
  if (field == nullptr) {
    return {};
  }

  return field->get_ref(object);
}

indirect_view reflection_field::create_indirect_view(const void* object) const {
  return get_ref(object).create_indirect_view();
}

indirect_view reflection_field::create_indirect_view(void* object) const {
  return get_ref(object).create_indirect_view();
}

bool reflection_field::try_set_value(void* object, const any& source) const {
  const reflection_field_data* field = info();
  if (field == nullptr) {
    return false;
  }

  return field->try_set_value(object, source);
}

object* reflection_field::get_object_ref(const void* object) const {
  const reflection_field_data* field = info();
  if (field == nullptr || field->get_object_ref == nullptr) {
    return nullptr;
  }

  return field->get_object_ref(field->data(object));
}

bool reflection_field::try_set_object_ref(void* object, caors::object* instance) const {
  const reflection_field_data* field = info();
  if (field == nullptr || field->set_object_ref == nullptr) {
    return false;
  }

  return field->set_object_ref(field->mutable_data(object), instance);
}

bool reflection_field::is_sequence() const noexcept { return kind() == type_kind::sequence; }

bool reflection_field::is_set() const noexcept { return kind() == type_kind::set; }

bool reflection_field::is_associative() const noexcept { return kind() == type_kind::associative; }

bool reflection_field::is_pair() const noexcept { return kind() == type_kind::pair; }

bool reflection_field::is_tuple() const noexcept { return kind() == type_kind::tuple; }

bool reflection_field::is_variant() const noexcept { return kind() == type_kind::variant; }

reflection_type::reflection_type(meta_type type, const reflection_type_data* data) noexcept
    : type_(type), data_(data) {}

bool reflection_type::valid() const noexcept {
  return type_.valid() && data_ != nullptr && data_->valid();
}

meta_type reflection_type::info() const noexcept { return valid() ? type_ : meta_type{}; }

type_id reflection_type::id() const noexcept { return type_.id(); }

string_name reflection_type::name() const noexcept { return type_.name(); }

type_guid reflection_type::guid() const noexcept { return type_.guid(); }

std::size_t reflection_type::field_count() const noexcept {
  const reflection_type_data* reflected = data();
  return reflected != nullptr ? reflected->fields.size() : 0;
}

std::optional<reflection_field> reflection_type::field_at(std::size_t index) const {
  const reflection_type_data* reflected = data();
  if (reflected == nullptr || index >= reflected->fields.size()) {
    return std::nullopt;
  }

  return reflection_field{type_, &reflected->fields[index]};
}

std::optional<reflection_field> reflection_type::find_field(string_name name) const {
  const reflection_type_data* reflected = data();
  if (reflected == nullptr || !name) {
    return std::nullopt;
  }

  for (std::size_t index = 0; index < reflected->fields.size(); ++index) {
    if (reflected->fields[index].name == name) {
      return field_at(index);
    }
  }

  return std::nullopt;
}

std::optional<reflection_field> reflection_type::find_field(std::string_view name) const {
  const std::optional<string_name> interned_name = string_name::find(name);
  if (!interned_name.has_value()) {
    return std::nullopt;
  }

  return find_field(*interned_name);
}

const reflection_type_data* reflection_type::data() const noexcept {
  if (!valid()) {
    return nullptr;
  }

  return data_;
}

reflection_registry::reflection_registry() = default;

reflection_registry::~reflection_registry() = default;

reflection_registry& reflection_registry::instance() noexcept {
  static reflection_registry registry;
  return registry;
}

error reflection_registry::register_type(const meta_type_data& type,
                                         const reflection_type_data& reflection) {
  return register_type(nullptr, type, reflection);
}

error reflection_registry::register_type(type_id id, const reflection_type_data& reflection) {
  if (!id) {
    return {error_domain::reflection, errc::invalid_type};
  }

  const std::optional<meta_type> stored_meta = get_meta_registry().find_type(id);
  if (!stored_meta.has_value()) {
    return {error_domain::reflection, errc::missing_type};
  }

  if (!reflection.valid()) {
    return {error_domain::reflection, errc::invalid_field};
  }

  reflection_type_data stored_reflection = reflection;

  const error field_status = resolve_fields(get_meta_registry(), stored_reflection);
  if (field_status) {
    return field_status;
  }

  const error validation_status = validate_registered_fields(stored_reflection);
  if (validation_status) {
    return validation_status;
  }

  return rethrow_as(get_meta_registry().emplace<reflection_type_data>(id, stored_reflection),
                    error_domain::reflection, errc::duplicate_component,
                    errc::duplicate_reflection);
}

error reflection_registry::register_type(meta_type type, const reflection_type_data& reflection) {
  if (!type.valid()) {
    return {error_domain::reflection, errc::invalid_type};
  }

  return register_type(type.id(), reflection);
}

error reflection_registry::register_type(detail::cpp_type_key key, const meta_type_data& type,
                                         const reflection_type_data& reflection) {
  if (!type.valid()) {
    return {error_domain::reflection, errc::invalid_type};
  }

  if (!reflection.valid()) {
    return {error_domain::reflection, errc::invalid_field};
  }

  std::optional<meta_type> existing;
  if (type.id()) {
    existing = get_meta_registry().find_type(type.id());
  }
  if (!existing.has_value()) {
    existing = get_meta_registry().find_type(type.name());
  }
  if (!existing.has_value() && type.guid()) {
    existing = get_meta_registry().find_type(type.guid());
  }

  if (existing.has_value()) {
    if (!same_type_data(*existing, type)) {
      return {error_domain::reflection, errc::inconsistent_type};
    }
  }

  const error type_status = key != nullptr ? get_meta_registry().register_cpp_type(key, type)
                                           : get_meta_registry().register_type(type);
  if (type_status && type_status.value() != errc::duplicate_id) {
    return rethrow_as(type_status, error_domain::reflection);
  }

  const std::optional<meta_type> stored_meta = key != nullptr
                                                   ? get_meta_registry().find_cpp_type(key)
                                                   : get_meta_registry().find_type(type.name());
  if (!stored_meta.has_value()) {
    return {error_domain::reflection, errc::invalid_type};
  }

  reflection_type_data stored_reflection = reflection;

  const error field_status = resolve_fields(get_meta_registry(), stored_reflection);
  if (field_status) {
    return field_status;
  }

  const error validation_status = validate_registered_fields(stored_reflection);
  if (validation_status) {
    return validation_status;
  }

  return rethrow_as(
      get_meta_registry().emplace<reflection_type_data>(stored_meta->id(), stored_reflection),
      error_domain::reflection, errc::duplicate_component, errc::duplicate_reflection);
}

bool reflection_registry::contains(type_id id) const {
  return get_meta_registry().has<reflection_type_data>(id);
}

const reflection_type_data* reflection_registry::find_reflection_data(type_id id) const {
  return get_meta_registry().find_ptr<reflection_type_data>(id);
}

std::optional<meta_type> reflection_registry::find_registered_type(detail::cpp_type_key key) const {
  return get_meta_registry().find_cpp_type(key);
}

std::optional<reflection_type> reflection_registry::find_type(type_id id) const {
  const std::optional<meta_type> type = get_meta_registry().find_type(id);
  if (!type.has_value()) {
    return std::nullopt;
  }

  const reflection_type_data* reflection = find_reflection_data(id);
  if (reflection == nullptr) {
    return std::nullopt;
  }

  return reflection_type{*type, reflection};
}

std::optional<reflection_type> reflection_registry::find_type(type_guid guid) const {
  const std::optional<meta_type> type = get_meta_registry().find_type(guid);
  if (!type.has_value()) {
    return std::nullopt;
  }

  return find_type(type->id());
}

std::optional<reflection_type> reflection_registry::find_type(std::string_view name) const {
  const std::optional<meta_type> type = get_meta_registry().find_type(name);
  if (!type.has_value()) {
    return std::nullopt;
  }

  return find_type(type->id());
}

std::optional<reflection_type> reflection_registry::find_type(string_name name) const {
  const std::optional<meta_type> type = get_meta_registry().find_type(name);
  if (!type.has_value()) {
    return std::nullopt;
  }

  return find_type(type->id());
}

std::size_t reflection_registry::size() const noexcept {
  return get_meta_registry().count<reflection_type_data>();
}

reflection_registry& get_reflection_registry() noexcept { return reflection_registry::instance(); }

error register_reflection(type_id id, reflection_type_data data) {
  return get_reflection_registry().register_type(id, data);
}

error register_reflection(meta_type type, reflection_type_data data) {
  return get_reflection_registry().register_type(type, data);
}

std::optional<reflection_type> find_reflection(type_id id) {
  return get_reflection_registry().find_type(id);
}

std::optional<reflection_type> find_reflection(type_guid guid) {
  return get_reflection_registry().find_type(guid);
}

std::optional<reflection_type> find_reflection(std::string_view name) {
  return get_reflection_registry().find_type(name);
}

}
