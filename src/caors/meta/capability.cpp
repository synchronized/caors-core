// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta/capability.h"

#include "caors/meta/detail/type_descriptor.h"
#include "caors/meta/registry.h"
#include "caors/meta/type.h"

#include "detail/registry_state.h"

#include <mutex>
#include <utility>

namespace caors::meta::detail {

error register_capability(type_id id, capability_key key, const void* capability) {
  if (!id) {
    return {error_domain::meta, errc::invalid_type};
  }

  if (key == nullptr || capability == nullptr) {
    return {error_domain::meta, errc::invalid_argument};
  }

  registry& registry = get_registry();
  std::lock_guard lock(registry.state_->mutex);
  type_data* type = find_type_data(*registry.state_, id);
  if (type == nullptr) {
    return {error_domain::meta, errc::missing_type};
  }

  return type->add_capability(key, capability);
}

error register_owned_capability(type_id id, capability_key key,
                                memory::unique_ptr<owned_capability_base> capability) {
  if (!id) {
    return {error_domain::meta, errc::invalid_type};
  }

  if (key == nullptr || capability == nullptr || capability->data() == nullptr) {
    return {error_domain::meta, errc::invalid_argument};
  }

  registry& registry = get_registry();
  std::lock_guard lock(registry.state_->mutex);
  type_data* type = find_type_data(*registry.state_, id);
  if (type == nullptr) {
    return {error_domain::meta, errc::missing_type};
  }

  return type->add_owned_capability(key, std::move(capability));
}

const void* find_capability(type_id id, capability_key key) noexcept {
  if (!id || key == nullptr) {
    return nullptr;
  }

  registry& registry = get_registry();
  std::lock_guard lock(registry.state_->mutex);
  const type_data* type = find_type_data(*registry.state_, id);
  if (type == nullptr) {
    return nullptr;
  }

  return type->find_capability(key);
}

const void* find_type_capability(const meta::type& type, capability_key key) noexcept {
  if (!type || key == nullptr) {
    return nullptr;
  }

  if (type.descriptor_ != nullptr) {
    if (const void* builtin = descriptor_builtin_ops(type.descriptor_, key); builtin != nullptr) {
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

  registry& registry = get_registry();
  std::lock_guard lock(registry.state_->mutex);
  std::size_t result = 0;
  for (const auto& [raw_id, type] : registry.state_->types) {
    (void)raw_id;
    if (type != nullptr && type->contains_capability(key)) {
      ++result;
    }
  }

  return result;
}

} // namespace caors::meta::detail
