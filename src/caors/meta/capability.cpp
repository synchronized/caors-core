// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta/capability.h"

#include "caors/meta/detail/type_access.h"
#include "caors/meta/registry.h"
#include "caors/meta/type.h"

#include "detail/registry_state.h"

#include <mutex>
#include <utility>

namespace caors::meta::detail {

const void* find_type_static_capability(const meta::type& type, capability_key key) noexcept {
  const type_record* data = type_access::data_of(type);
  if (data == nullptr || key == nullptr) {
    return nullptr;
  }

  return data->find_static_capability(key);
}

error register_capability(registry& registry, type_id id, capability_key key,
                          const void* capability) {
  if (!id) {
    return {error_domain::meta, errc::invalid_type};
  }

  if (key == nullptr || capability == nullptr) {
    return {error_domain::meta, errc::invalid_argument};
  }

  registry_state* state = registry_access::state(registry);
  std::lock_guard lock(state->mutex);
  type_record* type = find_type_record(*state, id);
  if (type == nullptr) {
    return {error_domain::meta, errc::missing_type};
  }

  return type->add_capability(key, capability);
}

error register_capability(type_id id, capability_key key, const void* capability) {
  return register_capability(get_registry(), id, key, capability);
}

error register_owned_capability(registry& registry, type_id id, capability_key key,
                                memory::unique_ptr<owned_capability_base> capability) {
  if (!id) {
    return {error_domain::meta, errc::invalid_type};
  }

  if (key == nullptr || capability == nullptr || capability->data() == nullptr) {
    return {error_domain::meta, errc::invalid_argument};
  }

  registry_state* state = registry_access::state(registry);
  std::lock_guard lock(state->mutex);
  type_record* type = find_type_record(*state, id);
  if (type == nullptr) {
    return {error_domain::meta, errc::missing_type};
  }

  return type->add_owned_capability(key, std::move(capability));
}

error register_owned_capability(type_id id, capability_key key,
                                memory::unique_ptr<owned_capability_base> capability) {
  return register_owned_capability(get_registry(), id, key, std::move(capability));
}

const void* find_capability(const registry& registry, type_id id, capability_key key) noexcept {
  if (!id || key == nullptr) {
    return nullptr;
  }

  const auto snapshot = registry_access::snapshot(registry);
  const registry_state& state = snapshot->state();
  std::lock_guard lock(state.mutex);
  const type_record* type = find_type_record(state, id);
  if (type == nullptr) {
    return nullptr;
  }

  return type->find_capability(key);
}

const void* find_capability(type_id id, capability_key key) noexcept {
  return find_capability(get_registry(), id, key);
}

const void* find_type_capability(const meta::type& type, capability_key key) noexcept {
  if (!type || key == nullptr) {
    return nullptr;
  }

  const type_record* data = type_access::data_of(type);
  if (data != nullptr) {
    if (const void* static_capability = data->find_static_capability(key);
        static_capability != nullptr) {
      return static_capability;
    }
  }

  const registry* source_registry = type_access::registry_of(type);
  if (source_registry == nullptr) {
    return nullptr;
  }

  return find_capability(*source_registry, type.id(), key);
}

bool contains_capability(const registry& registry, type_id id, capability_key key) noexcept {
  return find_capability(registry, id, key) != nullptr;
}

bool contains_capability(type_id id, capability_key key) noexcept {
  return contains_capability(get_registry(), id, key);
}

std::size_t capability_count(const registry& registry, capability_key key) noexcept {
  if (key == nullptr) {
    return 0;
  }

  const auto snapshot = registry_access::snapshot(registry);
  const registry_state& state = snapshot->state();
  std::lock_guard lock(state.mutex);
  std::size_t result = 0;
  for (const auto& [raw_id, type] : state.types) {
    (void)raw_id;
    if (type != nullptr && type->contains_capability(key)) {
      ++result;
    }
  }

  return result;
}

std::size_t capability_count(capability_key key) noexcept {
  return capability_count(get_registry(), key);
}

} // namespace caors::meta::detail
