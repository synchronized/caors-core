// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "meta_internal.h"

#include <mutex>
#include <string_view>

namespace caors::meta {

bool registry::contains(type_id id) const {
  std::lock_guard lock(state_->mutex);
  return state_->types.contains(id.value());
}

type registry::find_type(type_id id) const {
  std::lock_guard lock(state_->mutex);
  const detail::type_data* type_data = detail::find_type_data(*state_, id);
  if (type_data == nullptr) {
    return {};
  }

  return type{this, type_data};
}

type registry::find_type(core::type_guid guid) const {
  if (!guid) {
    return {};
  }

  std::lock_guard lock(state_->mutex);
  const auto guid_position = state_->guids.find(guid.value);
  if (guid_position == state_->guids.end()) {
    return {};
  }

  const detail::type_data* type_data = detail::find_type_data(*state_, guid_position->second);
  if (type_data == nullptr) {
    return {};
  }

  return type{this, type_data};
}

type registry::find_type(std::string_view name) const {
  std::optional<core::string_name> interned_name = core::string_name::find(name);
  if (!interned_name.has_value()) {
    return {};
  }

  return find_type(*interned_name);
}

type registry::find_type(core::string_name name) const {
  std::lock_guard lock(state_->mutex);
  const auto name_position = state_->names.find(name);
  if (name_position == state_->names.end()) {
    return {};
  }

  const detail::type_data* type_data = detail::find_type_data(*state_, name_position->second);
  if (type_data == nullptr) {
    return {};
  }

  return type{this, type_data};
}

bool registry::is_a(type_id id, type_id base_id) const {
  if (!id || !base_id) {
    return false;
  }

  std::lock_guard lock(state_->mutex);
  type_id current_id = id;
  while (current_id) {
    if (current_id == base_id) {
      return true;
    }

    const detail::type_data* type = detail::find_type_data(*state_, current_id);
    if (type == nullptr) {
      return false;
    }

    current_id = type->parent_id();
  }

  return false;
}

std::size_t registry::size() const noexcept {
  std::lock_guard lock(state_->mutex);
  return state_->types.size();
}

} // namespace caors::meta
