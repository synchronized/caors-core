// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "detail/registry_state.h"

#include "caors/meta/registry.h"

#include <mutex>
#include <utility>

namespace caors::meta {
namespace {

[[nodiscard]] bool contains_all_extensions(const detail::type_data& type,
                                           const detail::extension_key* keys,
                                           std::size_t count) noexcept {
  for (std::size_t index = 0; index < count; ++index) {
    if (keys[index] == nullptr || !type.contains_extension(keys[index])) {
      return false;
    }
  }

  return true;
}

} // namespace

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

  return stored->add_extension(std::move(extension));
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

core::vector<type_id> registry::view_extension_ids(const detail::extension_key* keys,
                                                   std::size_t count) const {
  core::vector<type_id> empty{core::vector<type_id>::allocator_type{state_->resource}};
  if (keys == nullptr || count == 0) {
    return empty;
  }

  std::lock_guard lock(state_->mutex);
  core::vector<type_id> result{core::vector<type_id>::allocator_type{state_->resource}};
  result.reserve(state_->types.size());

  for (const auto& [raw_id, stored] : state_->types) {
    (void)raw_id;
    if (stored != nullptr && stored->registered() &&
        contains_all_extensions(*stored, keys, count)) {
      result.push_back(stored->id());
    }
  }

  return result;
}

} // namespace caors::meta
