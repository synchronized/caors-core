// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/type_info.h"

#include <mutex>
#include <string>
#include <unordered_map>

namespace caors {
namespace {

struct type_info_entry {
  std::string name;
  type_info info;
};

type_registry default_type_registry;

} // namespace

class type_registry_state {
public:
  mutable std::mutex mutex;
  std::unordered_map<std::uint64_t, type_info_entry> types;
  std::unordered_map<std::string, type_id> names;
};

type_registry::type_registry() : state_(new type_registry_state) {}

type_registry::~type_registry() { delete state_; }

type_register_result type_registry::register_type(const type_info& type) {
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

bool type_registry::contains(type_id id) const {
  std::lock_guard lock(state_->mutex);
  return state_->types.contains(id.value());
}

std::optional<type_info> type_registry::find_type(type_id id) const {
  std::lock_guard lock(state_->mutex);
  const auto position = state_->types.find(id.value());
  if (position == state_->types.end()) {
    return std::nullopt;
  }

  return position->second.info;
}

std::optional<type_info> type_registry::find_type(std::string_view name) const {
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

bool type_registry::is_a(type_id id, type_id base_id) const {
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

std::size_t type_registry::size() const noexcept {
  std::lock_guard lock(state_->mutex);
  return state_->types.size();
}

void type_registry::clear() noexcept {
  std::lock_guard lock(state_->mutex);
  state_->types.clear();
  state_->names.clear();
}

type_registry& get_type_registry() noexcept { return default_type_registry; }

}
