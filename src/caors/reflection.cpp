// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/reflection.h"

#include <cstdint>
#include <unordered_set>

namespace caors {
namespace {

reflection_registry default_reflection_registry;

[[nodiscard]] bool same_type_info(const type_info& lhs, const type_info& rhs) noexcept {
  return lhs.name == rhs.name && lhs.guid == rhs.guid && lhs.size == rhs.size &&
         lhs.alignment == rhs.alignment && lhs.parent_id == rhs.parent_id;
}

[[nodiscard]] reflection_register_result
to_reflection_register_result(type_register_result result) noexcept {
  switch (result) {
  case type_register_result::success:
    return reflection_register_result::success;
  case type_register_result::invalid_type:
    return reflection_register_result::invalid_type;
  case type_register_result::duplicate_id:
    return reflection_register_result::duplicate_id;
  case type_register_result::duplicate_name:
    return reflection_register_result::duplicate_name;
  case type_register_result::duplicate_guid:
    return reflection_register_result::duplicate_guid;
  case type_register_result::missing_parent:
    return reflection_register_result::missing_parent;
  }

  return reflection_register_result::invalid_type;
}

[[nodiscard]] reflection_register_result
to_reflection_register_result(meta_component_result result) noexcept {
  switch (result) {
  case meta_component_result::success:
    return reflection_register_result::success;
  case meta_component_result::invalid_type:
    return reflection_register_result::invalid_type;
  case meta_component_result::missing_type:
    return reflection_register_result::invalid_type;
  case meta_component_result::duplicate_component:
    return reflection_register_result::duplicate_reflection;
  }

  return reflection_register_result::invalid_type;
}

[[nodiscard]] bool resolve_field_type(meta_registry& meta, field_info& field) {
  if (!field.valid()) {
    return false;
  }

  if (field.type && meta.contains(field.type)) {
    return true;
  }

  std::optional<type_info> resolved_type = meta.find_type(field.type_name);
  if (!resolved_type.has_value() && field.cpp_type != nullptr) {
    const type_info field_type{
        make_type_id(field.type_name),
        field.type_name,
        type_guid::nil(),
        field.size,
        field.alignment,
        type_id::invalid(),
    };

    const type_register_result result = meta.register_cpp_type(field.cpp_type, field_type);
    if (type_register_succeeded(result) || result == type_register_result::duplicate_id) {
      resolved_type = meta.find_cpp_type(field.cpp_type);
    }
  }

  if (!resolved_type.has_value()) {
    return false;
  }

  field.type = resolved_type->id;
  field.type_name = resolved_type->name;
  return true;
}

[[nodiscard]] reflection_register_result resolve_fields(meta_registry& meta,
                                                        reflection_type& type) {
  for (field_info& field : type.fields) {
    if (!resolve_field_type(meta, field)) {
      return reflection_register_result::invalid_field;
    }
  }

  return reflection_register_result::success;
}

[[nodiscard]] reflection_register_result validate_registered_fields(const reflection_type& type) {
  std::unordered_set<std::uint64_t> names;
  names.reserve(type.fields.size());

  for (const field_info& field : type.fields) {
    if (!field.valid() || !field.type) {
      return reflection_register_result::invalid_field;
    }

    if (!names.insert(field.name.id()).second) {
      return reflection_register_result::duplicate_field;
    }
  }

  return reflection_register_result::success;
}

} // namespace

bool reflection_type::valid() const noexcept {
  if (!info.valid()) {
    return false;
  }

  for (const field_info& field : fields) {
    if (!field.valid()) {
      return false;
    }
  }

  return true;
}

std::optional<field_info> reflection_type::find_field(string_name name) const {
  if (!name) {
    return std::nullopt;
  }

  for (const field_info& field : fields) {
    if (field.name == name) {
      return field;
    }
  }

  return std::nullopt;
}

std::optional<field_info> reflection_type::find_field(std::string_view name) const {
  const std::optional<string_name> interned_name = string_name::find(name);
  if (!interned_name.has_value()) {
    return std::nullopt;
  }

  return find_field(*interned_name);
}

class reflection_registry_state {
public:
  meta_registry meta;
};

reflection_registry::reflection_registry() : state_(new reflection_registry_state) {}

reflection_registry::~reflection_registry() { delete state_; }

reflection_register_result reflection_registry::register_type(const reflection_type& type) {
  return register_type(nullptr, type);
}

reflection_register_result reflection_registry::register_type(detail::cpp_type_key key,
                                                              const reflection_type& type) {
  if (!type.info.valid()) {
    return reflection_register_result::invalid_type;
  }

  std::optional<type_info> existing;
  if (type.info.id) {
    existing = state_->meta.find_type(type.info.id);
  }
  if (!existing.has_value()) {
    existing = state_->meta.find_type(type.info.name);
  }
  if (!existing.has_value() && type.info.guid) {
    existing = state_->meta.find_type(type.info.guid);
  }

  if (existing.has_value()) {
    if (!same_type_info(*existing, type.info)) {
      return reflection_register_result::inconsistent_type;
    }
  }

  const type_register_result type_result = key != nullptr
                                               ? state_->meta.register_cpp_type(key, type.info)
                                               : state_->meta.register_type(type.info);
  if (!type_register_succeeded(type_result) && type_result != type_register_result::duplicate_id) {
    return to_reflection_register_result(type_result);
  }

  const std::optional<type_info> stored_info =
      key != nullptr ? state_->meta.find_cpp_type(key) : state_->meta.find_type(type.info.name);
  if (!stored_info.has_value()) {
    return reflection_register_result::invalid_type;
  }

  reflection_type stored_type = type;
  stored_type.info = *stored_info;

  const reflection_register_result field_result = resolve_fields(state_->meta, stored_type);
  if (!reflection_register_succeeded(field_result)) {
    return field_result;
  }

  const reflection_register_result validation_result = validate_registered_fields(stored_type);
  if (!reflection_register_succeeded(validation_result)) {
    return validation_result;
  }

  return to_reflection_register_result(
      state_->meta.emplace<reflection_type>(stored_type.info.id, stored_type));
}

bool reflection_registry::contains(type_id id) const {
  return state_->meta.has<reflection_type>(id);
}

std::optional<reflection_type> reflection_registry::find_type(type_id id) const {
  return state_->meta.find<reflection_type>(id);
}

std::optional<type_info> reflection_registry::find_registered_type(detail::cpp_type_key key) const {
  return state_->meta.find_cpp_type(key);
}

std::optional<reflection_type> reflection_registry::find_type(type_guid guid) const {
  const std::optional<type_info> info = state_->meta.find_type(guid);
  if (!info.has_value()) {
    return std::nullopt;
  }

  return find_type(info->id);
}

std::optional<reflection_type> reflection_registry::find_type(std::string_view name) const {
  const std::optional<type_info> info = state_->meta.find_type(name);
  if (!info.has_value()) {
    return std::nullopt;
  }

  return find_type(info->id);
}

std::optional<reflection_type> reflection_registry::find_type(string_name name) const {
  const std::optional<type_info> info = state_->meta.find_type(name);
  if (!info.has_value()) {
    return std::nullopt;
  }

  return find_type(info->id);
}

std::size_t reflection_registry::size() const noexcept {
  return state_->meta.count<reflection_type>();
}

void reflection_registry::clear() noexcept { state_->meta.clear(); }

reflection_registry& get_reflection_registry() noexcept { return default_reflection_registry; }

std::optional<reflection_type> find_reflection(type_id id) {
  return get_reflection_registry().find_type(id);
}

std::optional<reflection_type> find_reflection(std::string_view name) {
  return get_reflection_registry().find_type(name);
}

}
