// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta/capability.h"

#include "caors/core/containers.h"
#include "caors/meta/type.h"
#include "caors/meta/type_descriptor.h"

#include <cstdint>
#include <mutex>
#include <utility>

namespace caors::meta::detail {

namespace {

using stored_capability_key = std::uintptr_t;

[[nodiscard]] stored_capability_key store_key(capability_key key) noexcept {
  return reinterpret_cast<stored_capability_key>(key);
}

struct capability_registry_state {
  mutable std::mutex mutex;
  struct entry {
    const void* data = nullptr;
    memory::unique_ptr<owned_capability_base> owned{};
  };

  core::hash_map<std::uint64_t, core::hash_map<stored_capability_key, entry>> capabilities;
};

[[nodiscard]] capability_registry_state& capability_state() {
  static capability_registry_state state;
  return state;
}

} // namespace

error register_capability(type_id id, capability_key key, const void* capability) {
  if (!id) {
    return {error_domain::meta, errc::invalid_type};
  }

  if (key == nullptr || capability == nullptr) {
    return {error_domain::meta, errc::invalid_argument};
  }

  capability_registry_state& state = capability_state();
  std::lock_guard lock(state.mutex);
  auto type_entry = state.capabilities.try_emplace(id.value());
  capability_registry_state::entry entry{};
  entry.data = capability;
  auto capability_entry = type_entry.first->second.emplace(store_key(key), std::move(entry));
  if (!capability_entry.second) {
    return {error_domain::meta, errc::duplicate_component};
  }

  return {};
}

error register_owned_capability(type_id id, capability_key key,
                                memory::unique_ptr<owned_capability_base> capability) {
  if (!id) {
    return {error_domain::meta, errc::invalid_type};
  }

  if (key == nullptr || capability == nullptr || capability->data() == nullptr) {
    return {error_domain::meta, errc::invalid_argument};
  }

  capability_registry_state& state = capability_state();
  std::lock_guard lock(state.mutex);
  auto type_entry = state.capabilities.try_emplace(id.value());
  capability_registry_state::entry entry{};
  entry.data = capability->data();
  entry.owned = std::move(capability);
  auto capability_entry = type_entry.first->second.emplace(store_key(key), std::move(entry));
  if (!capability_entry.second) {
    return {error_domain::meta, errc::duplicate_component};
  }

  return {};
}

const void* find_capability(type_id id, capability_key key) noexcept {
  if (!id || key == nullptr) {
    return nullptr;
  }

  capability_registry_state& state = capability_state();
  std::lock_guard lock(state.mutex);
  const auto type_position = state.capabilities.find(id.value());
  if (type_position == state.capabilities.end()) {
    return nullptr;
  }

  const auto capability_position = type_position->second.find(store_key(key));
  if (capability_position == type_position->second.end()) {
    return nullptr;
  }

  return capability_position->second.data;
}

const void* find_type_capability(const meta::type& type, capability_key key) noexcept {
  if (!type || key == nullptr) {
    return nullptr;
  }

  if (type.descriptor_ != nullptr) {
    if (const void* builtin = descriptor_builtin_capability(type.descriptor_, key);
        builtin != nullptr) {
      return builtin;
    }

    for (std::size_t index = 0; index < type.descriptor_->static_capability_count; ++index) {
      const capability_entry& entry = type.descriptor_->static_capabilities[index];
      if (entry.key == key && entry.data != nullptr) {
        return entry.data;
      }
    }
  }

  return find_capability(type.id(), key);
}

bool contains_capability(type_id id, capability_key key) noexcept {
  return find_capability(id, key) != nullptr;
}

std::size_t capability_count(capability_key key) noexcept {
  if (key == nullptr) {
    return 0;
  }

  const stored_capability_key stored_key = store_key(key);
  capability_registry_state& state = capability_state();
  std::lock_guard lock(state.mutex);
  std::size_t result = 0;
  for (const auto& type_entry : state.capabilities) {
    if (type_entry.second.contains(stored_key)) {
      ++result;
    }
  }

  return result;
}

} // namespace caors::meta::detail
