// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/reflection.h"

namespace caors {
namespace {

[[nodiscard]] bool same_type_data(meta::type_view lhs, const meta::type_data& rhs) noexcept {
  return lhs.name() == rhs.name() && lhs.guid() == rhs.guid() && lhs.size() == rhs.size() &&
         lhs.alignment() == rhs.alignment() && lhs.parent_id() == rhs.parent_id();
}

[[nodiscard]] bool resolve_field_type(meta::registry& meta, reflection::field_data& field) {
  if (!field.valid()) {
    return false;
  }

  if (field.type && meta.contains(field.type)) {
    return true;
  }

  return false;
}

[[nodiscard]] error resolve_fields(meta::registry& meta, reflection::type_data& reflection) {
  for (reflection::field_data& field : reflection.fields) {
    if (!resolve_field_type(meta, field)) {
      return {error_domain::reflection, errc::invalid_field};
    }
  }

  return {};
}

[[nodiscard]] error validate_registered_fields(const reflection::type_data& reflection) {
  hash_set<string_name> names;
  names.reserve(reflection.fields.size());

  for (const reflection::field_data& field : reflection.fields) {
    if (!field.valid() || !field.type) {
      return {error_domain::reflection, errc::invalid_field};
    }

    if (!names.insert(field.name).second) {
      return {error_domain::reflection, errc::duplicate_field};
    }
  }

  return {};
}

[[nodiscard]] std::optional<meta::type_view> field_meta(const reflection::field_data* field) {
  if (field == nullptr || !field->type) {
    return std::nullopt;
  }

  return caors::meta::find(field->type);
}

[[nodiscard]] const meta::type_semantics_data*
field_type_semantics(const reflection::field_data* field) noexcept {
  if (field == nullptr || !field->type) {
    return nullptr;
  }

  return meta::get_registry().find_ptr<meta::type_semantics_data>(field->type);
}

[[nodiscard]] string_name type_name_of(type_id id) noexcept {
  const std::optional<meta::type_view> type = caors::meta::find(id);
  return type.has_value() ? type->name() : string_name{};
}

} // namespace

bool reflection::type_data::valid() const noexcept {
  for (const reflection::field_data& field : fields) {
    if (!field.valid()) {
      return false;
    }
  }

  return true;
}

std::optional<reflection::field_data> reflection::type_data::find_field(string_name name) const {
  if (!name) {
    return std::nullopt;
  }

  for (const reflection::field_data& field : fields) {
    if (field.name == name) {
      return field;
    }
  }

  return std::nullopt;
}

std::optional<reflection::field_data>
reflection::type_data::find_field(std::string_view name) const {
  const std::optional<string_name> interned_name = string_name::find(name);
  if (!interned_name.has_value()) {
    return std::nullopt;
  }

  return find_field(*interned_name);
}

reflection::field::field(meta::type_view owner, const reflection::field_data* field) noexcept
    : owner_(owner), field_(field) {}

bool reflection::field::valid() const noexcept {
  return owner_.valid() && field_ != nullptr && field_->valid();
}

string_name reflection::field::name() const noexcept {
  const reflection::field_data* field = info();
  return field != nullptr ? field->name : string_name{};
}

type_id reflection::field::type() const noexcept {
  const reflection::field_data* field = info();
  return field != nullptr ? field->type : type_id{};
}

string_name reflection::field::type_name() const noexcept {
  const std::optional<meta::type_view> meta = field_meta(info());
  return meta.has_value() ? meta->name() : string_name{};
}

type_id reflection::field::value_type() const noexcept {
  const meta::type_semantics_data* traits = field_type_semantics(info());
  return traits != nullptr ? traits->value_type : type_id{};
}

type_id reflection::field::element_type() const noexcept {
  const meta::type_semantics_data* traits = field_type_semantics(info());
  return traits != nullptr ? traits->value_type : type_id{};
}

string_name reflection::field::element_type_name() const noexcept {
  return type_name_of(element_type());
}

type_id reflection::field::key_type() const noexcept {
  const meta::type_semantics_data* traits = field_type_semantics(info());
  return traits != nullptr ? traits->key_type : type_id{};
}

string_name reflection::field::key_type_name() const noexcept { return type_name_of(key_type()); }

type_id reflection::field::first_type() const noexcept { return key_type(); }

string_name reflection::field::first_type_name() const noexcept {
  return type_name_of(first_type());
}

type_id reflection::field::mapped_type() const noexcept {
  const meta::type_semantics_data* traits = field_type_semantics(info());
  return traits != nullptr ? traits->value_type : type_id{};
}

string_name reflection::field::mapped_type_name() const noexcept {
  return type_name_of(mapped_type());
}

type_id reflection::field::second_type() const noexcept { return value_type(); }

string_name reflection::field::second_type_name() const noexcept {
  return type_name_of(second_type());
}

meta::type_kind reflection::field::kind() const noexcept {
  const meta::type_semantics_data* traits = field_type_semantics(info());
  return traits != nullptr ? traits->kind : meta::type_kind::value;
}

meta::wrapper_semantics reflection::field::wrapper_semantics() const noexcept {
  const meta::type_semantics_data* traits = field_type_semantics(info());
  return traits != nullptr ? traits->wrapper_semantics : meta::wrapper_semantics::none;
}

std::uint8_t reflection::field::pointer_depth() const noexcept {
  const meta::type_semantics_data* traits = field_type_semantics(info());
  return traits != nullptr ? traits->pointer_depth : 0;
}

bool reflection::field::nullable() const noexcept {
  const meta::type_semantics_data* traits = field_type_semantics(info());
  return traits != nullptr && traits->nullable;
}

bool reflection::field::writable() const noexcept {
  const reflection::field_data* field = info();
  return field != nullptr && field->writable;
}

std::size_t reflection::field::offset() const noexcept {
  const reflection::field_data* field = info();
  return field != nullptr ? field->offset : 0;
}

std::size_t reflection::field::size() const noexcept {
  const std::optional<meta::type_view> meta = field_meta(info());
  return meta.has_value() ? meta->size() : 0;
}

std::size_t reflection::field::alignment() const noexcept {
  const std::optional<meta::type_view> meta = field_meta(info());
  return meta.has_value() ? meta->alignment() : 0;
}

const reflection::field_data* reflection::field::info() const noexcept {
  if (!valid()) {
    return nullptr;
  }

  return field_;
}

any reflection::field::get_value(const void* object) const {
  const reflection::field_data* field = info();
  if (field == nullptr) {
    return {};
  }

  return field->get_value(object);
}

any_ref reflection::field::get_ref(const void* object) const {
  const reflection::field_data* field = info();
  if (field == nullptr) {
    return {};
  }

  return field->get_ref(object);
}

any_ref reflection::field::get_ref(void* object) const {
  const reflection::field_data* field = info();
  if (field == nullptr) {
    return {};
  }

  return field->get_ref(object);
}

indirect_view reflection::field::create_indirect_view(const void* object) const {
  return get_ref(object).create_indirect_view();
}

indirect_view reflection::field::create_indirect_view(void* object) const {
  return get_ref(object).create_indirect_view();
}

bool reflection::field::try_set_value(void* object, const any& source) const {
  const reflection::field_data* field = info();
  if (field == nullptr) {
    return false;
  }

  return field->try_set_value(object, source);
}

object* reflection::field::get_object_ref(const void* object) const {
  const reflection::field_data* field = info();
  if (field == nullptr || field->get_object_ref == nullptr) {
    return nullptr;
  }

  return field->get_object_ref(field->data(object));
}

bool reflection::field::try_set_object_ref(void* object, caors::object* instance) const {
  const reflection::field_data* field = info();
  if (field == nullptr || field->set_object_ref == nullptr) {
    return false;
  }

  return field->set_object_ref(field->mutable_data(object), instance);
}

bool reflection::field::is_sequence() const noexcept { return kind() == meta::type_kind::sequence; }

bool reflection::field::is_set() const noexcept { return kind() == meta::type_kind::set; }

bool reflection::field::is_associative() const noexcept {
  return kind() == meta::type_kind::associative;
}

bool reflection::field::is_pair() const noexcept { return kind() == meta::type_kind::pair; }

bool reflection::field::is_tuple() const noexcept { return kind() == meta::type_kind::tuple; }

bool reflection::field::is_variant() const noexcept { return kind() == meta::type_kind::variant; }

reflection::type::type(meta::type_view type, const reflection::type_data* data) noexcept
    : type_(type), data_(data) {}

bool reflection::type::valid() const noexcept {
  return type_.valid() && data_ != nullptr && data_->valid();
}

meta::type_view reflection::type::info() const noexcept {
  return valid() ? type_ : meta::type_view{};
}

type_id reflection::type::id() const noexcept { return type_.id(); }

string_name reflection::type::name() const noexcept { return type_.name(); }

type_guid reflection::type::guid() const noexcept { return type_.guid(); }

std::size_t reflection::type::field_count() const noexcept {
  const reflection::type_data* reflected = data();
  return reflected != nullptr ? reflected->fields.size() : 0;
}

std::optional<reflection::field> reflection::type::field_at(std::size_t index) const {
  const reflection::type_data* reflected = data();
  if (reflected == nullptr || index >= reflected->fields.size()) {
    return std::nullopt;
  }

  return reflection::field{type_, &reflected->fields[index]};
}

std::optional<reflection::field> reflection::type::find_field(string_name name) const {
  const reflection::type_data* reflected = data();
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

std::optional<reflection::field> reflection::type::find_field(std::string_view name) const {
  const std::optional<string_name> interned_name = string_name::find(name);
  if (!interned_name.has_value()) {
    return std::nullopt;
  }

  return find_field(*interned_name);
}

const reflection::type_data* reflection::type::data() const noexcept {
  if (!valid()) {
    return nullptr;
  }

  return data_;
}

reflection::registry::registry() = default;

reflection::registry::~registry() = default;

reflection::registry& reflection::registry::instance() noexcept {
  static reflection::registry registry;
  return registry;
}

error reflection::registry::register_type(const meta::type_data& type,
                                          const reflection::type_data& reflection) {
  return register_type(nullptr, type, reflection);
}

error reflection::registry::register_type(type_id id, const reflection::type_data& reflection) {
  if (!id) {
    return {error_domain::reflection, errc::invalid_type};
  }

  const std::optional<meta::type_view> stored_meta = meta::get_registry().find_type(id);
  if (!stored_meta.has_value()) {
    return {error_domain::reflection, errc::missing_type};
  }

  if (!reflection.valid()) {
    return {error_domain::reflection, errc::invalid_field};
  }

  reflection::type_data stored_reflection = reflection;

  const error field_status = resolve_fields(meta::get_registry(), stored_reflection);
  if (field_status) {
    return field_status;
  }

  const error validation_status = validate_registered_fields(stored_reflection);
  if (validation_status) {
    return validation_status;
  }

  return rethrow_as(meta::get_registry().emplace<reflection::type_data>(id, stored_reflection),
                    error_domain::reflection, errc::duplicate_component,
                    errc::duplicate_reflection);
}

error reflection::registry::register_type(meta::type_view type,
                                          const reflection::type_data& reflection) {
  if (!type.valid()) {
    return {error_domain::reflection, errc::invalid_type};
  }

  return register_type(type.id(), reflection);
}

error reflection::registry::register_type(detail::cpp_type_key key, const meta::type_data& type,
                                          const reflection::type_data& reflection) {
  if (!type.valid()) {
    return {error_domain::reflection, errc::invalid_type};
  }

  if (!reflection.valid()) {
    return {error_domain::reflection, errc::invalid_field};
  }

  std::optional<meta::type_view> existing;
  if (type.id()) {
    existing = meta::get_registry().find_type(type.id());
  }
  if (!existing.has_value()) {
    existing = meta::get_registry().find_type(type.name());
  }
  if (!existing.has_value() && type.guid()) {
    existing = meta::get_registry().find_type(type.guid());
  }

  if (existing.has_value()) {
    if (!same_type_data(*existing, type)) {
      return {error_domain::reflection, errc::inconsistent_type};
    }
  }

  const error type_status = key != nullptr ? meta::get_registry().register_cpp_type(key, type)
                                           : meta::get_registry().register_type(type);
  if (type_status && type_status.value() != errc::duplicate_id) {
    return rethrow_as(type_status, error_domain::reflection);
  }

  const std::optional<meta::type_view> stored_meta =
      key != nullptr ? meta::get_registry().find_cpp_type(key)
                     : meta::get_registry().find_type(type.name());
  if (!stored_meta.has_value()) {
    return {error_domain::reflection, errc::invalid_type};
  }

  reflection::type_data stored_reflection = reflection;

  const error field_status = resolve_fields(meta::get_registry(), stored_reflection);
  if (field_status) {
    return field_status;
  }

  const error validation_status = validate_registered_fields(stored_reflection);
  if (validation_status) {
    return validation_status;
  }

  return rethrow_as(
      meta::get_registry().emplace<reflection::type_data>(stored_meta->id(), stored_reflection),
      error_domain::reflection, errc::duplicate_component, errc::duplicate_reflection);
}

bool reflection::registry::contains(type_id id) const {
  return meta::get_registry().has<reflection::type_data>(id);
}

const reflection::type_data* reflection::registry::find_reflection_data(type_id id) const {
  return meta::get_registry().find_ptr<reflection::type_data>(id);
}

std::optional<meta::type_view>
reflection::registry::find_registered_type(detail::cpp_type_key key) const {
  return meta::get_registry().find_cpp_type(key);
}

std::optional<reflection::type> reflection::registry::find_type(type_id id) const {
  const std::optional<meta::type_view> type = meta::get_registry().find_type(id);
  if (!type.has_value()) {
    return std::nullopt;
  }

  const reflection::type_data* reflection = find_reflection_data(id);
  if (reflection == nullptr) {
    return std::nullopt;
  }

  return reflection::type{*type, reflection};
}

std::optional<reflection::type> reflection::registry::find_type(type_guid guid) const {
  const std::optional<meta::type_view> type = meta::get_registry().find_type(guid);
  if (!type.has_value()) {
    return std::nullopt;
  }

  return find_type(type->id());
}

std::optional<reflection::type> reflection::registry::find_type(std::string_view name) const {
  const std::optional<meta::type_view> type = meta::get_registry().find_type(name);
  if (!type.has_value()) {
    return std::nullopt;
  }

  return find_type(type->id());
}

std::optional<reflection::type> reflection::registry::find_type(string_name name) const {
  const std::optional<meta::type_view> type = meta::get_registry().find_type(name);
  if (!type.has_value()) {
    return std::nullopt;
  }

  return find_type(type->id());
}

std::size_t reflection::registry::size() const noexcept {
  return meta::get_registry().count<reflection::type_data>();
}

reflection::registry& reflection::get_registry() noexcept {
  return reflection::registry::instance();
}

namespace detail {

error register_reflection(type_id id, reflection::type_data data) {
  return reflection::get_registry().register_type(id, data);
}

error register_reflection(meta::type_view type, reflection::type_data data) {
  return reflection::get_registry().register_type(type, data);
}

} // namespace detail

namespace reflection {

std::optional<reflection::type> find(type_id id) {
  return reflection::get_registry().find_type(id);
}

std::optional<reflection::type> find(type_guid guid) {
  return reflection::get_registry().find_type(guid);
}

std::optional<reflection::type> find(std::string_view name) {
  return reflection::get_registry().find_type(name);
}

bool contains(type_id id) { return reflection::get_registry().contains(id); }

std::size_t size() noexcept { return reflection::get_registry().size(); }

} // namespace reflection

} // namespace caors
