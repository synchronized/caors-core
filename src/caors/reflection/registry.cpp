// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/reflection.h"

namespace caors::reflection {
namespace {

[[nodiscard]] error validate_reflection_type(type_id id,
                                             const detail::type_data& reflection) noexcept {
  if (!id) {
    return {error_domain::reflection, errc::invalid_type};
  }

  const meta::type reflected_type = reflection.get_type();
  if (!reflected_type || reflected_type.id() != id) {
    return {error_domain::reflection, errc::invalid_type};
  }

  return {};
}

[[nodiscard]] bool resolve_field_type(meta::registry& registry, detail::field_wrapper& field) {
  if (!field.valid()) {
    return false;
  }

  const meta::type type = field.get_type();
  if (type && type.registered() && registry.contains(type.id())) {
    return true;
  }

  return static_cast<bool>(type);
}

[[nodiscard]] error resolve_fields(meta::registry& meta, detail::type_data& reflection) {
  for (detail::field_wrapper& field : reflection.fields) {
    if (!resolve_field_type(meta, field)) {
      return {error_domain::reflection, errc::invalid_field};
    }
  }

  return {};
}

[[nodiscard]] error validate_registered_fields(const detail::type_data& reflection) {
  core::hash_set<core::string_name> names;
  names.reserve(reflection.fields.size());

  for (const detail::field_wrapper& field : reflection.fields) {
    if (!field.valid() || !field.get_type()) {
      return {error_domain::reflection, errc::invalid_field};
    }

    if (!names.insert(field.name()).second) {
      return {error_domain::reflection, errc::duplicate_field};
    }
  }

  return {};
}

[[nodiscard]] error validate_registered_methods(const detail::type_data& reflection) {
  core::hash_set<core::string_name> names;
  names.reserve(reflection.methods.size());

  for (const detail::method_wrapper& method : reflection.methods) {
    if (!method.valid()) {
      return {error_domain::reflection, errc::invalid_field};
    }

    if (!names.insert(method.name()).second) {
      return {error_domain::reflection, errc::duplicate_method};
    }
  }

  return {};
}

[[nodiscard]] const detail::type_data* as_type_data(const void* reflection) noexcept {
  return static_cast<const detail::type_data*>(reflection);
}

} // namespace

registry::registry() = default;

registry::~registry() = default;

registry& registry::instance() noexcept {
  static registry registry;
  return registry;
}

error registry::attach_type(meta::type type, const void* reflection) {
  if (!type.registered()) {
    return {error_domain::reflection, errc::invalid_type};
  }

  const detail::type_data* reflection_data = as_type_data(reflection);
  if (reflection_data == nullptr || !reflection_data->valid()) {
    return {error_domain::reflection, errc::invalid_field};
  }

  const error reflected_type_status = validate_reflection_type(type.id(), *reflection_data);
  if (reflected_type_status) {
    return reflected_type_status;
  }

  detail::type_data stored_reflection = *reflection_data;

  const error field_status = resolve_fields(meta::get_registry(), stored_reflection);
  if (field_status) {
    return field_status;
  }

  const error validation_status = validate_registered_fields(stored_reflection);
  if (validation_status) {
    return validation_status;
  }

  const error method_status = validate_registered_methods(stored_reflection);
  if (method_status) {
    return method_status;
  }

  return rethrow_as(meta::get_registry().emplace<detail::type_data>(type.id(), stored_reflection),
                    error_domain::reflection, errc::duplicate_component,
                    errc::duplicate_reflection);
}

bool registry::contains(type_id id) const {
  return meta::get_registry().has<detail::type_data>(id);
}

const void* registry::find_reflection_data(type_id id) const {
  return meta::get_registry().find_ptr<detail::type_data>(id);
}

type registry::find_type(type_id id) const {
  const meta::type meta_type = meta::get_registry().find_type(id);
  if (!meta_type) {
    return {};
  }

  const detail::type_data* reflection = as_type_data(find_reflection_data(id));
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
