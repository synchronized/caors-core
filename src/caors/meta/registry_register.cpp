// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "detail/registry_state.h"

#include "caors/meta/detail/type_descriptor.h"

#include <mutex>
#include <typeindex>
#include <utility>

namespace caors::meta {
namespace {

[[nodiscard]] error register_type_impl(detail::registry_state& state, detail::type_data type,
                                       type_id* stored_id = nullptr) {
  if (!type.valid() || !type.id()) {
    return {error_domain::meta, errc::invalid_type};
  }

  std::lock_guard lock(state.mutex);
  error conflict{};
  const std::optional<type_id> existing_id = detail::find_existing_type_id(state, type, conflict);
  if (conflict) {
    return conflict;
  }

  if (existing_id.has_value()) {
    detail::type_data* existing = detail::find_type_data(state, *existing_id);
    if (existing == nullptr) {
      return {error_domain::meta, errc::invalid_type};
    }

    const error status = detail::merge_type_data(state, *existing, type);
    if (status.value() == errc::invalid_type) {
      return detail::duplicate_identity_error(state, type, *existing_id);
    }

    if (stored_id != nullptr) {
      *stored_id = existing->id();
    }

    return status;
  }

  return detail::store_new_type(state, type, stored_id);
}

[[nodiscard]] error validate_reflection_type(type_id id, detail::reflection_type_ops ops) noexcept {
  if (!id) {
    return {error_domain::reflection, errc::invalid_type};
  }

  const meta::type reflected_type = ops.get_type != nullptr ? ops.get_type() : meta::type{};
  if (!reflected_type || reflected_type.id() != id) {
    return {error_domain::reflection, errc::invalid_type};
  }

  return {};
}

[[nodiscard]] bool resolve_field_type(const detail::field_data& field) {
  if (!field.valid()) {
    return false;
  }

  const meta::type type = field.get_type();
  if (type && type.registered() && get_registry().contains(type.id())) {
    return true;
  }

  return static_cast<bool>(type);
}

[[nodiscard]] error resolve_fields(const detail::type_data& reflection) {
  for (std::size_t index = 0; index < reflection.field_count(); ++index) {
    const detail::field_data* field = reflection.field_at(index);
    if (field == nullptr || !resolve_field_type(*field)) {
      return {error_domain::reflection, errc::invalid_field};
    }
  }

  return {};
}

[[nodiscard]] error validate_registered_fields(const detail::type_data& reflection) {
  core::hash_set<core::string_name> names;
  names.reserve(reflection.field_count());

  for (std::size_t index = 0; index < reflection.field_count(); ++index) {
    const detail::field_data* field = reflection.field_at(index);
    if (field == nullptr || !field->valid() || !field->get_type()) {
      return {error_domain::reflection, errc::invalid_field};
    }

    if (!names.insert(field->name()).second) {
      return {error_domain::reflection, errc::duplicate_field};
    }
  }

  return {};
}

[[nodiscard]] error validate_registered_methods(const detail::type_data& reflection) {
  core::hash_set<core::string_name> names;
  names.reserve(reflection.method_count());

  for (std::size_t index = 0; index < reflection.method_count(); ++index) {
    const detail::method_data* method = reflection.method_at(index);
    if (method == nullptr || !method->valid()) {
      return {error_domain::reflection, errc::invalid_field};
    }

    if (!names.insert(method->name()).second) {
      return {error_domain::reflection, errc::duplicate_method};
    }
  }

  return {};
}

[[nodiscard]] error validate_reflection_storage(type_id id, const detail::type_data& reflection) {
  if (!id || !reflection.has_reflection()) {
    return {error_domain::reflection, errc::invalid_type};
  }

  const meta::type reflected_type = reflection.get_type();
  if (!reflected_type || reflected_type.id() != id) {
    return {error_domain::reflection, errc::invalid_type};
  }

  if (!reflection.reflection_valid()) {
    return {error_domain::reflection, errc::invalid_field};
  }

  const error field_status = resolve_fields(reflection);
  if (field_status) {
    return field_status;
  }

  const error field_validation_status = validate_registered_fields(reflection);
  if (field_validation_status) {
    return field_validation_status;
  }

  return validate_registered_methods(reflection);
}

} // namespace

error registry::register_type(const type_registration_data& type) {
  return register_type_impl(*state_, type);
}

error registry::ensure_type_descriptor(const detail::type_descriptor& descriptor) {
  std::lock_guard lock(state_->mutex);
  if (!descriptor.valid() || !descriptor.type) {
    return {error_domain::meta, errc::invalid_type};
  }

  if (detail::find_type_descriptor(*state_, descriptor.type) != nullptr) {
    return detail::store_type_descriptor(*state_, descriptor);
  }

  return detail::store_type_descriptor(*state_, descriptor);
}

namespace detail {

error ensure_type_descriptor_registered(const type_descriptor& descriptor) {
  return get_registry().ensure_type_descriptor(descriptor);
}

} // namespace detail

error registry::set_type_guid(type_id id, type_guid guid) {
  if (!id) {
    return {error_domain::meta, errc::invalid_type};
  }

  std::lock_guard lock(state_->mutex);
  detail::type_data* stored = detail::find_type_data(*state_, id);
  if (stored == nullptr) {
    return {error_domain::meta, errc::invalid_type};
  }

  if (!guid) {
    return {error_domain::meta, errc::duplicate_id};
  }

  if (stored->guid()) {
    return stored->guid() == guid ? error{error_domain::meta, errc::duplicate_id}
                                  : error{error_domain::meta, errc::invalid_type};
  }

  if (state_->guids.contains(guid.value)) {
    return {error_domain::meta, errc::duplicate_guid};
  }

  stored->set_guid(guid);
  state_->guids.emplace(guid.value, id);
  return {};
}

error registry::register_cpp_type_info(const std::type_info& info, type_id id) {
  if (!id) {
    return {error_domain::meta, errc::invalid_type};
  }

  std::lock_guard lock(state_->mutex);
  if (!state_->types.contains(id.value())) {
    return {error_domain::meta, errc::missing_type};
  }

  const std::type_index index{info};
  const auto position = state_->cpp_type_ids.find(index);
  if (position != state_->cpp_type_ids.end()) {
    return position->second == id ? error{} : error{error_domain::meta, errc::invalid_type};
  }

  state_->cpp_type_ids.emplace(index, id);
  return {};
}

error registry::attach_reflection(type_id id, const detail::type_data& reflection) {
  if (!id) {
    return {error_domain::reflection, errc::invalid_type};
  }

  const error validation_status = validate_reflection_storage(id, reflection);
  if (validation_status) {
    return validation_status;
  }

  std::lock_guard lock(state_->mutex);
  detail::type_data* stored = detail::find_type_data(*state_, id);
  if (stored == nullptr) {
    return {error_domain::reflection, errc::missing_type};
  }

  return stored->attach_reflection(reflection);
}

detail::type_data* registry::ensure_reflection(type_id id, detail::reflection_type_ops ops) {
  if (!id) {
    return nullptr;
  }

  const error validation_status = validate_reflection_type(id, ops);
  if (validation_status) {
    return nullptr;
  }

  std::lock_guard lock(state_->mutex);
  detail::type_data* stored = detail::find_type_data(*state_, id);
  if (stored == nullptr) {
    return nullptr;
  }

  return stored->ensure_reflection(ops);
}

error registry::set_type_parent(type_id id, type_id parent_id) {
  return set_type_parent(id, parent_id, {});
}

error registry::set_type_parent(type_id id, type_id parent_id, detail::parent_cast_ops cast) {
  if (!id) {
    return {error_domain::meta, errc::invalid_type};
  }

  std::lock_guard lock(state_->mutex);
  detail::type_data* stored = detail::find_type_data(*state_, id);
  if (stored == nullptr) {
    return {error_domain::meta, errc::invalid_type};
  }

  if (!parent_id) {
    return {error_domain::meta, errc::duplicate_id};
  }

  if (cast.parent && cast.parent != parent_id) {
    return {error_domain::meta, errc::invalid_type};
  }

  if (!state_->types.contains(parent_id.value())) {
    return {error_domain::meta, errc::missing_parent};
  }

  bool changed = false;
  if (stored->parent_id()) {
    if (stored->parent_id() != parent_id) {
      return {error_domain::meta, errc::invalid_type};
    }
  } else {
    stored->set_parent_id(parent_id);
    changed = true;
  }

  if (cast.valid()) {
    const auto position = state_->parent_casts.find(id.value());
    if (position != state_->parent_casts.end()) {
      if (position->second.parent != parent_id ||
          position->second.const_upcast != cast.const_upcast ||
          position->second.mutable_upcast != cast.mutable_upcast) {
        return {error_domain::meta, errc::invalid_type};
      }
    } else {
      state_->parent_casts.emplace(id.value(), cast);
      changed = true;
    }
  }

  return changed ? error{} : error{error_domain::meta, errc::duplicate_id};
}

} // namespace caors::meta
