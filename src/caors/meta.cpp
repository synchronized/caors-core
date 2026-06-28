// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta.h"

#include <mutex>
#include <unordered_map>

namespace caors {
namespace {

meta_registry default_meta_registry;

} // namespace

class meta_registry_state {
public:
  mutable std::mutex mutex;
  type_registry types;
  std::unordered_map<detail::meta_component_key,
                     std::unique_ptr<detail::meta_component_storage_base>>
      components;
};

meta_registry::meta_registry() : state_(new meta_registry_state) {}

meta_registry::~meta_registry() { delete state_; }

type_register_result meta_registry::register_type(const type_info& type) {
  return state_->types.register_type(type);
}

bool meta_registry::contains(type_id id) const { return state_->types.contains(id); }

std::optional<type_info> meta_registry::find_type(type_id id) const {
  return state_->types.find_type(id);
}

std::optional<type_info> meta_registry::find_type(std::string_view name) const {
  return state_->types.find_type(name);
}

bool meta_registry::is_a(type_id id, type_id base_id) const {
  return state_->types.is_a(id, base_id);
}

std::size_t meta_registry::size() const noexcept { return state_->types.size(); }

void meta_registry::clear() noexcept {
  state_->types.clear();

  std::lock_guard lock(state_->mutex);
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
