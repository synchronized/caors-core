// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "meta_internal.h"

#include "caors/meta/detail/type_descriptor.h"

#include <mutex>

namespace caors::meta {
namespace {

[[nodiscard]] error register_type_impl(registry_state& state, detail::type_data type,
                                       type_id* stored_id = nullptr) {
  if (!type.valid() || !type.id()) {
    return {error_domain::meta, errc::invalid_type};
  }

  std::lock_guard lock(state.mutex);
  if (state.frozen) {
    return {error_domain::meta, errc::frozen};
  }

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

} // namespace

error registry::register_type(const detail::type_data& type) {
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

  if (state_->frozen) {
    return {error_domain::meta, errc::frozen};
  }

  return detail::store_type_descriptor(*state_, descriptor);
}

error registry::set_type_guid(type_id id, type_guid guid) {
  if (!id) {
    return {error_domain::meta, errc::invalid_type};
  }

  std::lock_guard lock(state_->mutex);
  if (state_->frozen) {
    return {error_domain::meta, errc::frozen};
  }

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

error registry::set_type_parent(type_id id, type_id parent_id) {
  if (!id) {
    return {error_domain::meta, errc::invalid_type};
  }

  std::lock_guard lock(state_->mutex);
  if (state_->frozen) {
    return {error_domain::meta, errc::frozen};
  }

  detail::type_data* stored = detail::find_type_data(*state_, id);
  if (stored == nullptr) {
    return {error_domain::meta, errc::invalid_type};
  }

  if (!parent_id) {
    return {error_domain::meta, errc::duplicate_id};
  }

  if (!state_->types.contains(parent_id.value())) {
    return {error_domain::meta, errc::missing_parent};
  }

  if (stored->parent_id()) {
    return stored->parent_id() == parent_id ? error{error_domain::meta, errc::duplicate_id}
                                            : error{error_domain::meta, errc::invalid_type};
  }

  stored->set_parent_id(parent_id);
  return {};
}

} // namespace caors::meta
