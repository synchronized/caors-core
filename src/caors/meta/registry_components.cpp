// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "detail/internal.h"

#include <mutex>

namespace caors::meta {

detail::meta_component_storage_base*
registry::find_component_storage(detail::meta_component_key key) {
  std::lock_guard lock(state_->mutex);
  const auto position = state_->components.find(key);
  if (position == state_->components.end()) {
    return nullptr;
  }

  return position->second.get();
}

const detail::meta_component_storage_base*
registry::find_component_storage(detail::meta_component_key key) const {
  std::lock_guard lock(state_->mutex);
  const auto position = state_->components.find(key);
  if (position == state_->components.end()) {
    return nullptr;
  }

  return position->second.get();
}

detail::meta_component_storage_base&
registry::ensure_component_storage(detail::meta_component_key key,
                                   std::unique_ptr<detail::meta_component_storage_base> storage) {
  std::lock_guard lock(state_->mutex);
  auto [position, inserted] = state_->components.try_emplace(key, nullptr);
  if (inserted) {
    position->second = std::move(storage);
  }

  return *position->second;
}

core::vector<type_id> registry::view_component_ids(const detail::meta_component_key* keys,
                                                   std::size_t count) const {
  if (keys == nullptr || count == 0) {
    return {};
  }

  std::lock_guard lock(state_->mutex);
  const auto first_position = state_->components.find(keys[0]);
  if (first_position == state_->components.end()) {
    return {};
  }

  core::vector<type_id> candidate_ids = first_position->second->ids();
  core::vector<type_id> result;
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

} // namespace caors::meta
