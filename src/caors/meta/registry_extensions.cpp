// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "detail/registry_state.h"

#include "caors/meta/registry.h"

#include <mutex>
#include <utility>

namespace caors::meta {
namespace {

[[nodiscard]] bool contains_all_extensions(const detail::type_record& type,
                                           const detail::extension_key* keys,
                                           std::size_t count) noexcept {
  for (std::size_t index = 0; index < count; ++index) {
    if (!keys[index] || !type.contains_extension(keys[index])) {
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
  if (state_->frozen) {
    return {error_domain::meta, errc::frozen};
  }
  detail::type_record* stored = detail::find_type_record(*state_, id);
  if (stored == nullptr || !stored->registered()) {
    return {error_domain::meta, errc::missing_type};
  }

  return stored->add_extension(std::move(extension));
}

bool registry::has_extension(type_id id, detail::extension_key key) const {
  if (!id || !key) {
    return false;
  }

  const auto snapshot = capture_snapshot();
  for (const auto& layer : snapshot->layers()) {
    const detail::registry_state& state = layer->state();
    std::lock_guard lock(state.mutex);
    const detail::type_record* stored = detail::find_type_record(state, id);
    if (stored != nullptr) {
      return stored->registered() && stored->contains_extension(key);
    }
  }
  return false;
}

const detail::extension_data* registry::find_extension_data(type_id id,
                                                            detail::extension_key key) const {
  if (!id || !key) {
    return nullptr;
  }

  const detail::registry_state& state = primary_layer_->state();
  std::lock_guard lock(state.mutex);
  const detail::type_record* stored = detail::find_type_record(state, id);
  if (stored != nullptr) {
    return stored->registered() ? stored->find_extension(key) : nullptr;
  }
  return nullptr;
}

bool registry::copy_extension_data(type_id id, detail::extension_key key, void* destination,
                                   extension_copy_fn copy) const {
  if (!id || !key || destination == nullptr || copy == nullptr) {
    return false;
  }

  const auto snapshot = capture_snapshot();
  for (const auto& layer : snapshot->layers()) {
    const detail::registry_state& state = layer->state();
    std::lock_guard lock(state.mutex);
    const detail::type_record* stored = detail::find_type_record(state, id);
    if (stored != nullptr) {
      const detail::extension_data* extension =
          stored->registered() ? stored->find_extension(key) : nullptr;
      if (extension == nullptr) {
        return false;
      }
      copy(destination, *extension);
      return true;
    }
  }
  return false;
}

core::vector<type_id>
registry::view_extension_ids(const std::shared_ptr<detail::registry_snapshot>& snapshot,
                             const detail::extension_key* keys, std::size_t count) const {
  core::vector<type_id> empty{core::vector<type_id>::allocator_type{&memory::library_resource()}};
  if (snapshot == nullptr || keys == nullptr || count == 0) {
    return empty;
  }

  core::vector<type_id> result{core::vector<type_id>::allocator_type{&memory::library_resource()}};
  for (const auto& layer : snapshot->layers()) {
    const detail::registry_state& state = layer->state();
    std::lock_guard lock(state.mutex);
    result.reserve(result.size() + state.types.size());
    for (const auto& [raw_id, stored] : state.types) {
      (void)raw_id;
      if (stored != nullptr && stored->registered() &&
          contains_all_extensions(*stored, keys, count)) {
        result.push_back(stored->id());
      }
    }
  }

  return result;
}

} // namespace caors::meta
