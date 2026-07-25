// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "detail/registry_state.h"

#include "caors/meta/detail/type_access.h"
#include "caors/meta/registry.h"

#include <mutex>
#include <string_view>

namespace caors::meta {

bool registry::contains(type_id id) const {
  const auto snapshot = capture_snapshot();
  const detail::registry_state& state = snapshot->state();
  std::lock_guard lock(state.mutex);
  return state.types.contains(id.value());
}

type registry::find_type(type_id id) const {
  const auto snapshot = capture_snapshot();
  const detail::registry_state& state = snapshot->state();
  std::lock_guard lock(state.mutex);
  const detail::type_record* type_record = detail::find_type_record(state, id);
  if (type_record == nullptr) {
    return {};
  }

  return detail::type_access::make(this, type_record);
}

type registry::find_type(core::type_guid guid) const {
  if (!guid) {
    return {};
  }

  const auto snapshot = capture_snapshot();
  const detail::registry_state& state = snapshot->state();
  std::lock_guard lock(state.mutex);
  const auto guid_position = state.guids.find(guid.value);
  if (guid_position == state.guids.end()) {
    return {};
  }

  const detail::type_record* type_record = guid_position->second;
  if (type_record == nullptr) {
    return {};
  }

  return detail::type_access::make(this, type_record);
}

type registry::find_type(const std::type_info& info) const {
  const auto snapshot = capture_snapshot();
  const detail::registry_state& state = snapshot->state();
  std::lock_guard lock(state.mutex);
  const auto position = state.cpp_types.find(std::type_index{info});
  if (position == state.cpp_types.end()) {
    return {};
  }

  const detail::type_record* type_record = position->second;
  if (type_record == nullptr) {
    return {};
  }

  return detail::type_access::make(this, type_record);
}

type registry::find_type(std::string_view name) const {
  std::optional<core::string_name> interned_name = core::string_name::find(name);
  if (!interned_name.has_value()) {
    return {};
  }

  return find_type(*interned_name);
}

type registry::find_type(core::string_name name) const {
  const auto snapshot = capture_snapshot();
  const detail::registry_state& state = snapshot->state();
  std::lock_guard lock(state.mutex);
  const auto name_position = state.names.find(name);
  if (name_position == state.names.end()) {
    return {};
  }

  const detail::type_record* type_record = name_position->second;
  if (type_record == nullptr) {
    return {};
  }

  return detail::type_access::make(this, type_record);
}

bool registry::is_a(type_id id, type_id base_id) const {
  if (!id || !base_id) {
    return false;
  }

  const auto snapshot = capture_snapshot();
  const detail::registry_state& state = snapshot->state();
  std::lock_guard lock(state.mutex);
  type_id current_id = id;
  while (current_id) {
    if (current_id == base_id) {
      return true;
    }

    const detail::type_record* type = detail::find_type_record(state, current_id);
    if (type == nullptr) {
      return false;
    }

    current_id = type->parent_id();
  }

  return false;
}

const void* registry::upcast(type_id id, type_id base_id, const void* value) const {
  if (!id || !base_id || value == nullptr) {
    return nullptr;
  }

  if (id == base_id) {
    return value;
  }

  const auto snapshot = capture_snapshot();
  const detail::registry_state& state = snapshot->state();
  std::lock_guard lock(state.mutex);
  type_id current_id = id;
  const void* current_value = value;
  while (current_id) {
    const detail::type_record* type = detail::find_type_record(state, current_id);
    if (type == nullptr) {
      return nullptr;
    }

    const type_id parent_id = type->parent_id();
    if (!parent_id) {
      return nullptr;
    }

    const detail::parent_cast_ops* cast = type->parent_cast();
    if (cast == nullptr || cast->parent != parent_id || cast->const_upcast == nullptr) {
      return nullptr;
    }

    current_value = cast->const_upcast(current_value);
    if (current_value == nullptr) {
      return nullptr;
    }

    if (parent_id == base_id) {
      return current_value;
    }

    current_id = parent_id;
  }

  return nullptr;
}

void* registry::upcast(type_id id, type_id base_id, void* value) const {
  if (!id || !base_id || value == nullptr) {
    return nullptr;
  }

  if (id == base_id) {
    return value;
  }

  const auto snapshot = capture_snapshot();
  const detail::registry_state& state = snapshot->state();
  std::lock_guard lock(state.mutex);
  type_id current_id = id;
  void* current_value = value;
  while (current_id) {
    const detail::type_record* type = detail::find_type_record(state, current_id);
    if (type == nullptr) {
      return nullptr;
    }

    const type_id parent_id = type->parent_id();
    if (!parent_id) {
      return nullptr;
    }

    const detail::parent_cast_ops* cast = type->parent_cast();
    if (cast == nullptr || cast->parent != parent_id || cast->mutable_upcast == nullptr) {
      return nullptr;
    }

    current_value = cast->mutable_upcast(current_value);
    if (current_value == nullptr) {
      return nullptr;
    }

    if (parent_id == base_id) {
      return current_value;
    }

    current_id = parent_id;
  }

  return nullptr;
}

std::size_t registry::size() const noexcept {
  const auto snapshot = capture_snapshot();
  const detail::registry_state& state = snapshot->state();
  std::lock_guard lock(state.mutex);
  return state.types.size();
}

} // namespace caors::meta
