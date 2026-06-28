// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta.h"

#include <mutex>
#include <string>
#include <unordered_map>

namespace caors {
namespace {

struct type_info_entry {
  std::string name;
  type_info info;
};

meta_registry default_meta_registry;

} // namespace

class meta_registry_state {
public:
  mutable std::mutex mutex;
  std::unordered_map<std::uint64_t, type_info_entry> types;
  std::unordered_map<std::string, type_id> names;
  std::unordered_map<detail::meta_component_key,
                     std::unique_ptr<detail::meta_component_storage_base>>
      components;
};

meta_registry::meta_registry() : state_(new meta_registry_state) {}

meta_registry::~meta_registry() { delete state_; }

type_register_result meta_registry::register_type(const type_info& type) {
  if (!type.valid()) {
    return type_register_result::invalid_type;
  }

  std::lock_guard lock(state_->mutex);
  const auto id_position = state_->types.find(type.id.value());
  if (id_position != state_->types.end()) {
    if (id_position->second.info.name == type.name) {
      return type_register_result::duplicate_id;
    }

    return type_register_result::hash_collision;
  }

  if (state_->names.contains(std::string(type.name))) {
    return type_register_result::duplicate_name;
  }

  if (type.parent_id && !state_->types.contains(type.parent_id.value())) {
    return type_register_result::missing_parent;
  }

  auto [position, inserted] = state_->types.try_emplace(type.id.value());
  if (!inserted) {
    return type_register_result::duplicate_id;
  }

  type_info_entry& entry = position->second;
  entry.name = std::string(type.name);
  entry.info = type;
  entry.info.name = entry.name;
  state_->names.emplace(entry.name, type.id);

  return type_register_result::success;
}

bool meta_registry::contains(type_id id) const {
  std::lock_guard lock(state_->mutex);
  return state_->types.contains(id.value());
}

std::optional<type_info> meta_registry::find_type(type_id id) const {
  std::lock_guard lock(state_->mutex);
  const auto position = state_->types.find(id.value());
  if (position == state_->types.end()) {
    return std::nullopt;
  }

  return position->second.info;
}

std::optional<type_info> meta_registry::find_type(std::string_view name) const {
  std::lock_guard lock(state_->mutex);
  const auto name_position = state_->names.find(std::string(name));
  if (name_position == state_->names.end()) {
    return std::nullopt;
  }

  const auto type_position = state_->types.find(name_position->second.value());
  if (type_position == state_->types.end()) {
    return std::nullopt;
  }

  return type_position->second.info;
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

    const auto position = state_->types.find(current_id.value());
    if (position == state_->types.end()) {
      return false;
    }

    current_id = position->second.info.parent_id;
  }

  return false;
}

std::size_t meta_registry::size() const noexcept {
  std::lock_guard lock(state_->mutex);
  return state_->types.size();
}

void meta_registry::clear() noexcept {
  std::lock_guard lock(state_->mutex);
  state_->types.clear();
  state_->names.clear();
  for (auto& [key, storage] : state_->components) {
    (void)key;
    storage->clear();
  }
}

detail::meta_component_storage_base*
meta_registry::find_component_storage(detail::meta_component_key key) {
  std::lock_guard lock(state_->mutex);
  const auto position = state_->components.find(key);
  if (position == state_->components.end()) {
    return nullptr;
  }

  return position->second.get();
}

const detail::meta_component_storage_base*
meta_registry::find_component_storage(detail::meta_component_key key) const {
  std::lock_guard lock(state_->mutex);
  const auto position = state_->components.find(key);
  if (position == state_->components.end()) {
    return nullptr;
  }

  return position->second.get();
}

detail::meta_component_storage_base& meta_registry::ensure_component_storage(
    detail::meta_component_key key, std::unique_ptr<detail::meta_component_storage_base> storage) {
  std::lock_guard lock(state_->mutex);
  auto [position, inserted] = state_->components.try_emplace(key, nullptr);
  if (inserted) {
    position->second = std::move(storage);
  }

  return *position->second;
}

std::vector<type_id> meta_registry::view_component_ids(const detail::meta_component_key* keys,
                                                       std::size_t count) const {
  if (keys == nullptr || count == 0) {
    return {};
  }

  std::lock_guard lock(state_->mutex);
  const auto first_position = state_->components.find(keys[0]);
  if (first_position == state_->components.end()) {
    return {};
  }

  std::vector<type_id> candidate_ids = first_position->second->ids();
  std::vector<type_id> result;
  result.reserve(candidate_ids.size());

  for (const type_id id : candidate_ids) {
    bool matched = true;
    for (std::size_t index = 1; index < count; ++index) {
      const auto position = state_->components.find(keys[index]);
      if (position == state_->components.end() || !position->second->contains(id)) {
        matched = false;
        break;
      }
    }

    if (matched) {
      result.push_back(id);
    }
  }

  return result;
}

meta_registry& get_meta_registry() noexcept { return default_meta_registry; }

}
