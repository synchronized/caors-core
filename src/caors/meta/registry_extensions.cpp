// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "detail/registry_state.h"

#include "caors/meta/registry.h"

#include <mutex>
#include <utility>

namespace caors::meta {
namespace {

enum class extension_copy_result {
  missing_type,
  missing_extension,
  copied,
  inactive,
};

struct extension_presence {
  bool type_found = false;
  bool extension_found = false;
};

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

[[nodiscard]] extension_copy_result
copy_extension_from_layer(const std::shared_ptr<detail::registry_layer_state>& layer, type_id id,
                          detail::extension_key key, void* destination,
                          void (*copy)(void*, const detail::extension_data&)) {
  if (layer == nullptr) {
    return extension_copy_result::missing_type;
  }

  const detail::registry_state& state = layer->state();
  if (layer->frozen() || state.frozen) {
    const detail::type_record* stored = detail::find_type_record(state, id);
    const detail::extension_data* extension =
        stored != nullptr && stored->registered() ? stored->find_extension(key) : nullptr;
    if (stored == nullptr) {
      return extension_copy_result::missing_type;
    }
    if (extension == nullptr) {
      return extension_copy_result::missing_extension;
    }

    const detail::layer_call_guard guard{layer};
    if (!guard) {
      return extension_copy_result::inactive;
    }
    copy(destination, *extension);
    return extension_copy_result::copied;
  }

  std::lock_guard lock(state.mutex);
  const detail::type_record* stored = detail::find_type_record(state, id);
  const detail::extension_data* extension =
      stored != nullptr && stored->registered() ? stored->find_extension(key) : nullptr;
  if (stored == nullptr) {
    return extension_copy_result::missing_type;
  }
  if (extension == nullptr) {
    return extension_copy_result::missing_extension;
  }

  copy(destination, *extension);
  return extension_copy_result::copied;
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
    const extension_presence found = detail::read_layer_state(*layer, [id, key](const auto& state) {
      const detail::type_record* stored = detail::find_type_record(state, id);
      return extension_presence{stored != nullptr, stored != nullptr && stored->registered() &&
                                                       stored->contains_extension(key)};
    });
    if (found.type_found) {
      return found.extension_found;
    }
  }
  return false;
}

const detail::extension_data* registry::find_extension_data(type_id id,
                                                            detail::extension_key key) const {
  if (!id || !key) {
    return nullptr;
  }

  return detail::read_layer_state(*primary_layer_, [id, key](const auto& state) {
    const detail::type_record* stored = detail::find_type_record(state, id);
    return stored != nullptr && stored->registered() ? stored->find_extension(key) : nullptr;
  });
}

bool registry::copy_extension_data(type_id id, detail::extension_key key, void* destination,
                                   extension_copy_fn copy) const {
  if (!id || !key || destination == nullptr || copy == nullptr) {
    return false;
  }

  const auto snapshot = capture_snapshot();
  for (const auto& layer : snapshot->layers()) {
    const extension_copy_result result =
        copy_extension_from_layer(layer, id, key, destination, copy);
    if (result == extension_copy_result::copied) {
      return true;
    }
    if (result != extension_copy_result::missing_type) {
      return false;
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
    detail::read_layer_state(*layer, [&](const auto& state) {
      result.reserve(result.size() + state.types.size());
      for (const auto& [raw_id, stored] : state.types) {
        (void)raw_id;
        if (stored != nullptr && stored->registered() &&
            contains_all_extensions(*stored, keys, count)) {
          result.push_back(stored->id());
        }
      }
    });
  }

  return result;
}

} // namespace caors::meta
