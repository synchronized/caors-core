// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "meta_internal.h"

#include <mutex>
#include <string_view>

namespace caors {

bool meta_registry::contains(type_id id) const {
  std::lock_guard lock(state_->mutex);
  return state_->types.contains(id.value());
}

std::optional<meta_type> meta_registry::find_cpp_type(detail::cpp_type_key key) const {
  if (key == nullptr) {
    return std::nullopt;
  }

  std::lock_guard lock(state_->mutex);
  const auto key_position = state_->cpp_types.find(key);
  if (key_position == state_->cpp_types.end()) {
    return std::nullopt;
  }

  const meta_type_data* type = detail::find_type_data(*state_, key_position->second);
  if (type == nullptr) {
    return std::nullopt;
  }

  return meta_type{this, type};
}

std::optional<meta_type> meta_registry::find_type(type_id id) const {
  std::lock_guard lock(state_->mutex);
  const meta_type_data* type = detail::find_type_data(*state_, id);
  if (type == nullptr) {
    return std::nullopt;
  }

  return meta_type{this, type};
}

std::optional<meta_type> meta_registry::find_type(type_guid guid) const {
  if (!guid) {
    return std::nullopt;
  }

  std::lock_guard lock(state_->mutex);
  const auto guid_position = state_->guids.find(guid.value);
  if (guid_position == state_->guids.end()) {
    return std::nullopt;
  }

  const meta_type_data* type = detail::find_type_data(*state_, guid_position->second);
  if (type == nullptr) {
    return std::nullopt;
  }

  return meta_type{this, type};
}

std::optional<meta_type> meta_registry::find_type(std::string_view name) const {
  std::optional<string_name> interned_name = string_name::find(name);
  if (!interned_name.has_value()) {
    return std::nullopt;
  }

  return find_type(*interned_name);
}

std::optional<meta_type> meta_registry::find_type(string_name name) const {
  std::lock_guard lock(state_->mutex);
  const auto name_position = state_->names.find(name.id());
  if (name_position == state_->names.end()) {
    return std::nullopt;
  }

  const meta_type_data* type = detail::find_type_data(*state_, name_position->second);
  if (type == nullptr) {
    return std::nullopt;
  }

  return meta_type{this, type};
}

bool meta_registry::is_a(type_id id, type_id base_id) const {
  if (!id || !base_id) {
    return false;
  }

  std::lock_guard lock(state_->mutex);
  type_id current_id = id;
  while (current_id) {
    if (current_id == base_id) {
      return true;
    }

    const meta_type_data* type = detail::find_type_data(*state_, current_id);
    if (type == nullptr) {
      return false;
    }

    current_id = type->parent_id();
  }

  return false;
}

std::size_t meta_registry::size() const noexcept {
  std::lock_guard lock(state_->mutex);
  return state_->types.size();
}

} // namespace caors
