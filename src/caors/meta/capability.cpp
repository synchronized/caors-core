// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta/capability.h"

#include "caors/meta/detail/type_access.h"
#include "caors/meta/detail/type_descriptor.h"
#include "caors/meta/registry.h"
#include "caors/meta/type.h"

#include "detail/registry_state.h"

#include <mutex>
#include <utility>

namespace caors::meta::detail {

const void* find_type_builtin_ops(const meta::type& type, builtin_ops_kind kind) noexcept {
  const type_descriptor* descriptor = type_access::descriptor_of(type);
  if (descriptor == nullptr) {
    return nullptr;
  }

  switch (kind) {
  case builtin_ops_kind::value_storage:
    return descriptor_ops<value_storage_ops>(*descriptor);
  case builtin_ops_kind::scalar:
    return descriptor_ops<scalar_ops>(*descriptor);
  case builtin_ops_kind::enumeration:
    return descriptor_ops<enum_ops>(*descriptor);
  case builtin_ops_kind::indirect:
    return descriptor_ops<indirect_ops>(*descriptor);
  case builtin_ops_kind::sequence:
    return descriptor_ops<sequence_ops>(*descriptor);
  case builtin_ops_kind::set:
    return descriptor_ops<set_ops>(*descriptor);
  case builtin_ops_kind::associative:
    return descriptor_ops<associative_ops>(*descriptor);
  case builtin_ops_kind::pair:
    return descriptor_ops<pair_ops>(*descriptor);
  case builtin_ops_kind::tuple:
    return descriptor_ops<tuple_ops>(*descriptor);
  case builtin_ops_kind::variant:
    return descriptor_ops<variant_ops>(*descriptor);
  }

  return nullptr;
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
  type_data* type = find_type_data(*state, id);
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
  type_data* type = find_type_data(*state, id);
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

  const registry_state* state = registry_access::state(registry);
  std::lock_guard lock(state->mutex);
  const type_data* type = find_type_data(*state, id);
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

  const type_descriptor* descriptor = type_access::descriptor_of(type);
  if (descriptor != nullptr) {
    if (const void* builtin = descriptor_builtin_ops(descriptor, key); builtin != nullptr) {
      return builtin;
    }

    for (std::size_t index = 0; index < descriptor->static_capability_count; ++index) {
      const capability_entry& entry = descriptor->static_capabilities[index];
      if (capability_key_equal(entry.key, key) && entry.data != nullptr) {
        return entry.data;
      }
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

  const registry_state* state = registry_access::state(registry);
  std::lock_guard lock(state->mutex);
  std::size_t result = 0;
  for (const auto& [raw_id, type] : state->types) {
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
