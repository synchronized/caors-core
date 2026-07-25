// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "detail/registry_state.h"

#include "caors/meta/detail/type_access.h"
#include "caors/meta/registry.h"

#include <mutex>
#include <string_view>

namespace caors::meta {
namespace {

[[nodiscard]] const detail::type_record* find_record(const detail::registry_snapshot& snapshot,
                                                     type_id id) {
  for (const auto& layer : snapshot.layers()) {
    const detail::registry_state& state = layer->state();
    std::lock_guard lock(state.mutex);
    if (const detail::type_record* record = detail::find_type_record(state, id)) {
      return record;
    }
  }
  return nullptr;
}

} // namespace

bool registry::contains(type_id id) const {
  const auto snapshot = capture_snapshot();
  return find_record(*snapshot, id) != nullptr;
}

type registry::find_type(type_id id) const {
  const auto snapshot = capture_snapshot();
  const detail::type_record* type_record = find_record(*snapshot, id);
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
  for (const auto& layer : snapshot->layers()) {
    const detail::registry_state& state = layer->state();
    std::lock_guard lock(state.mutex);
    const auto position = state.guids.find(guid.value);
    if (position != state.guids.end() && position->second != nullptr) {
      return detail::type_access::make(this, position->second);
    }
  }
  return {};
}

type registry::find_type(const std::type_info& info) const {
  const auto snapshot = capture_snapshot();
  for (const auto& layer : snapshot->layers()) {
    const detail::registry_state& state = layer->state();
    std::lock_guard lock(state.mutex);
    const auto position = state.cpp_types.find(std::type_index{info});
    if (position != state.cpp_types.end() && position->second != nullptr) {
      return detail::type_access::make(this, position->second);
    }
  }
  return {};
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
  for (const auto& layer : snapshot->layers()) {
    const detail::registry_state& state = layer->state();
    std::lock_guard lock(state.mutex);
    const auto position = state.names.find(name);
    if (position != state.names.end() && position->second != nullptr) {
      return detail::type_access::make(this, position->second);
    }
  }
  return {};
}

bool registry::is_a(type_id id, type_id base_id) const {
  if (!id || !base_id) {
    return false;
  }

  const auto snapshot = capture_snapshot();
  type_id current_id = id;
  while (current_id) {
    if (current_id == base_id) {
      return true;
    }

    const detail::type_record* type = find_record(*snapshot, current_id);
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
  type_id current_id = id;
  const void* current_value = value;
  while (current_id) {
    const detail::type_record* type = find_record(*snapshot, current_id);
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
  type_id current_id = id;
  void* current_value = value;
  while (current_id) {
    const detail::type_record* type = find_record(*snapshot, current_id);
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
  std::size_t result = 0;
  for (const auto& layer : snapshot->layers()) {
    const detail::registry_state& state = layer->state();
    std::lock_guard lock(state.mutex);
    result += state.types.size();
  }
  return result;
}

} // namespace caors::meta
