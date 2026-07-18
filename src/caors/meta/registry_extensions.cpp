// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "detail/registry_state.h"

#include <mutex>
#include <utility>

namespace caors::meta {

error registry::add_extension(type_id id, detail::extension_data extension) {
  if (!id) {
    return {error_domain::meta, errc::invalid_type};
  }

  if (!extension.valid()) {
    return {error_domain::meta, errc::invalid_argument};
  }

  std::lock_guard lock(state_->mutex);
  detail::type_data* stored = detail::find_type_data(*state_, id);
  if (stored == nullptr || !stored->registered()) {
    return {error_domain::meta, errc::missing_type};
  }

  const detail::extension_key key = extension.key;
  error status = stored->add_extension(std::move(extension));
  if (status) {
    return status;
  }

  return detail::index_type_extension(*state_, id, key);
}

bool registry::has_extension(type_id id, detail::extension_key key) const {
  if (!id || key == nullptr) {
    return false;
  }

  std::lock_guard lock(state_->mutex);
  const detail::type_data* stored = detail::find_type_data(*state_, id);
  return stored != nullptr && stored->registered() && stored->contains_extension(key);
}

const detail::extension_data* registry::find_extension_data(type_id id,
                                                            detail::extension_key key) const {
  if (!id || key == nullptr) {
    return nullptr;
  }

  std::lock_guard lock(state_->mutex);
  const detail::type_data* stored = detail::find_type_data(*state_, id);
  return stored != nullptr && stored->registered() ? stored->find_extension(key) : nullptr;
}

detail::extension_data* registry::find_extension_mutable_data(type_id id,
                                                              detail::extension_key key) {
  if (!id || key == nullptr) {
    return nullptr;
  }

  std::lock_guard lock(state_->mutex);
  detail::type_data* stored = detail::find_type_data(*state_, id);
  return stored != nullptr && stored->registered() ? stored->mutable_find_extension(key) : nullptr;
}

error registry::index_extension(type_id id, detail::extension_key key) {
  std::lock_guard lock(state_->mutex);
  return detail::index_type_extension(*state_, id, key);
}

core::vector<type_id> registry::view_extension_ids(const detail::extension_key* keys,
                                                   std::size_t count) const {
  core::vector<type_id> empty{core::vector<type_id>::allocator_type{state_->resource}};
  if (keys == nullptr || count == 0) {
    return empty;
  }

  std::lock_guard lock(state_->mutex);
  const auto first_position = state_->extension_index.find(keys[0]);
  if (first_position == state_->extension_index.end()) {
    return empty;
  }

  const core::vector<type_id>& candidate_ids = first_position->second;
  core::vector<type_id> result{core::vector<type_id>::allocator_type{state_->resource}};
  result.reserve(candidate_ids.size());

  for (const type_id id : candidate_ids) {
    bool matched = true;
    for (std::size_t index = 1; index < count; ++index) {
      const auto position = state_->extension_index.find(keys[index]);
      if (position == state_->extension_index.end()) {
        matched = false;
        break;
      }

      bool contains = false;
      for (const type_id candidate : position->second) {
        if (candidate == id) {
          contains = true;
          break;
        }
      }

      if (!contains) {
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
