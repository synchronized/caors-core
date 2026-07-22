// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "detail/registry_state.h"

#include "caors/meta/detail/type_descriptor.h"
#include "caors/meta/registry.h"

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

  return detail::store_new_type(state, std::move(type), stored_id);
}

} // namespace

error registry::register_type(type_id id, string_name name, type_guid guid, type_id parent_id) {
  return register_type_impl(*state_, detail::type_data{id, name, guid, parent_id});
}

error registry::commit_type(detail::type_data type, const std::type_info& cpp_type) {
  if (!type.valid() || !type.id()) {
    return {error_domain::meta, errc::invalid_type};
  }

  std::lock_guard lock(state_->mutex);
  if (state_->types.contains(type.id().value())) {
    return {error_domain::meta, errc::duplicate_id};
  }

  const type_id expected_id = type.id();
  error status = detail::store_new_type(*state_, std::move(type));
  if (status) {
    return status;
  }

  detail::type_data* stored = detail::find_type_data(*state_, expected_id);
  if (stored == nullptr) {
    return {error_domain::meta, errc::invalid_type};
  }

  const std::type_index cpp_index{cpp_type};
  const auto cpp_position = state_->cpp_types.find(cpp_index);
  if (cpp_position != state_->cpp_types.end() && cpp_position->second != stored) {
    return {error_domain::meta, errc::invalid_type};
  }
  state_->cpp_types.emplace(cpp_index, stored);

  for (std::size_t index = 0; index < stored->extension_count(); ++index) {
    status = detail::index_type_extension(*state_, expected_id, stored->extension_key_at(index));
    if (status) {
      return status;
    }
  }

  return {};
}

error registry::ensure_type_descriptor(const detail::type_descriptor& descriptor) {
  std::lock_guard lock(state_->mutex);
  if (!descriptor.valid() || !descriptor.type) {
    return {error_domain::meta, errc::invalid_type};
  }

  return detail::store_type_descriptor(*state_, descriptor);
}

namespace detail {

error ensure_type_descriptor_registered(const type_descriptor& descriptor) {
  return registry_access::ensure_type_descriptor(get_registry(), descriptor);
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
  state_->guids.emplace(guid.value, stored);
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
  const auto position = state_->cpp_types.find(index);
  if (position != state_->cpp_types.end()) {
    return position->second != nullptr && position->second->id() == id
               ? error{}
               : error{error_domain::meta, errc::invalid_type};
  }

  detail::type_data* stored = detail::find_type_data(*state_, id);
  if (stored == nullptr) {
    return {error_domain::meta, errc::missing_type};
  }

  state_->cpp_types.emplace(index, stored);
  return {};
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
    const detail::parent_cast_ops* existing_cast = stored->parent_cast();
    if (existing_cast != nullptr) {
      if (existing_cast->parent != parent_id || existing_cast->const_upcast != cast.const_upcast ||
          existing_cast->mutable_upcast != cast.mutable_upcast) {
        return {error_domain::meta, errc::invalid_type};
      }
    } else {
      stored->set_parent_cast(cast);
      changed = true;
    }
  }

  return changed ? error{} : error{error_domain::meta, errc::duplicate_id};
}

} // namespace caors::meta
