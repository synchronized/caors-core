// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "detail/internal.h"

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
  const type_descriptor* descriptor = detail::find_type_descriptor(*state_, id);
  if (type_data == nullptr && descriptor == nullptr) {
    return {};
  }

  return type{this, type_data, descriptor};
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

  return type{this, type_data, detail::find_type_descriptor(*state_, type_data->id())};
}

type registry::find_type(const std::type_info& info) const {
  std::lock_guard lock(state_->mutex);
  const auto position = state_->cpp_type_ids.find(std::type_index{info});
  if (position == state_->cpp_type_ids.end()) {
    return {};
  }

  const detail::type_data* type_data = detail::find_type_data(*state_, position->second);
  if (type_data == nullptr) {
    return {};
  }

  return type{this, type_data, detail::find_type_descriptor(*state_, type_data->id())};
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

  return type{this, type_data, detail::find_type_descriptor(*state_, type_data->id())};
}

const type_descriptor* registry::find_type_descriptor(type_id id) const {
  std::lock_guard lock(state_->mutex);
  return detail::find_type_descriptor(*state_, id);
}

value_factory_ops registry::find_type_factory(type_id id) const {
  std::lock_guard lock(state_->mutex);
  return detail::find_type_factory(*state_, id);
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

const void* registry::upcast(type_id id, type_id base_id, const void* value) const {
  if (!id || !base_id || value == nullptr) {
    return nullptr;
  }

  if (id == base_id) {
    return value;
  }

  std::lock_guard lock(state_->mutex);
  type_id current_id = id;
  const void* current_value = value;
  while (current_id) {
    const detail::type_data* type = detail::find_type_data(*state_, current_id);
    if (type == nullptr) {
      return nullptr;
    }

    const type_id parent_id = type->parent_id();
    if (!parent_id) {
      return nullptr;
    }

    const detail::parent_cast_ops* cast = detail::find_parent_cast(*state_, current_id);
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

  std::lock_guard lock(state_->mutex);
  type_id current_id = id;
  void* current_value = value;
  while (current_id) {
    const detail::type_data* type = detail::find_type_data(*state_, current_id);
    if (type == nullptr) {
      return nullptr;
    }

    const type_id parent_id = type->parent_id();
    if (!parent_id) {
      return nullptr;
    }

    const detail::parent_cast_ops* cast = detail::find_parent_cast(*state_, current_id);
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
  std::lock_guard lock(state_->mutex);
  return state_->types.size();
}

} // namespace caors::meta
