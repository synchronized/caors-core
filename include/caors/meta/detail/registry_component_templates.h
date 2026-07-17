// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_REGISTRY_COMPONENT_TEMPLATES_H_
#define CAORS_META_DETAIL_REGISTRY_COMPONENT_TEMPLATES_H_

#include "caors/meta/registry.h"

#include <optional>
#include <type_traits>
#include <utility>

namespace caors::meta {

template <typename Component, typename... Args>
[[nodiscard]] error registry::emplace_component(type_id id, Args&&... args) {
  using component_type = std::decay_t<Component>;

  if (!id) {
    return {error_domain::meta, errc::invalid_type};
  }

  if (!contains(id)) {
    return {error_domain::meta, errc::missing_type};
  }

  detail::meta_component_storage_base* storage_base =
      find_component_storage(detail::meta_component_key_of<component_type>());
  if (storage_base == nullptr) {
    storage_base = &ensure_component_storage(
        detail::meta_component_key_of<component_type>(),
        detail::make_meta_component_storage<component_type>(state_resource_));
  }

  auto& storage = static_cast<detail::meta_component_storage<component_type>&>(*storage_base);
  if (!storage.emplace(id, std::forward<Args>(args)...)) {
    return {error_domain::meta, errc::duplicate_component};
  }

  return {};
}

template <typename Component, typename... Args>
[[nodiscard]] const Component* registry::ensure_component(type_id id, Args&&... args) {
  if (const Component* existing = find_component_ptr<Component>(id); existing != nullptr) {
    return existing;
  }

  const error status = emplace_component<Component>(id, std::forward<Args>(args)...);
  if (status && status.value() != errc::duplicate_component) {
    return nullptr;
  }

  return find_component_ptr<Component>(id);
}

template <typename Component> [[nodiscard]] bool registry::has_component(type_id id) const {
  using component_type = std::decay_t<Component>;
  const detail::meta_component_storage_base* storage_base =
      find_component_storage(detail::meta_component_key_of<component_type>());
  if (storage_base == nullptr) {
    return false;
  }

  return storage_base->contains(id);
}

template <typename Component>
[[nodiscard]] std::optional<Component> registry::find_component(type_id id) const {
  using component_type = std::decay_t<Component>;
  const detail::meta_component_storage_base* storage_base =
      find_component_storage(detail::meta_component_key_of<component_type>());
  if (storage_base == nullptr) {
    return std::nullopt;
  }

  const auto& storage =
      static_cast<const detail::meta_component_storage<component_type>&>(*storage_base);
  return storage.find(id);
}

template <typename Component>
[[nodiscard]] const Component* registry::find_component_ptr(type_id id) const {
  using component_type = std::decay_t<Component>;
  const detail::meta_component_storage_base* storage_base =
      find_component_storage(detail::meta_component_key_of<component_type>());
  if (storage_base == nullptr) {
    return nullptr;
  }

  const auto& storage =
      static_cast<const detail::meta_component_storage<component_type>&>(*storage_base);
  return storage.find_ptr(id);
}

template <typename Component>
[[nodiscard]] Component* registry::find_component_mutable_ptr(type_id id) {
  using component_type = std::decay_t<Component>;
  detail::meta_component_storage_base* storage_base =
      find_component_storage(detail::meta_component_key_of<component_type>());
  if (storage_base == nullptr) {
    return nullptr;
  }

  auto& storage = static_cast<detail::meta_component_storage<component_type>&>(*storage_base);
  return storage.find_mutable_ptr(id);
}

template <typename Component> [[nodiscard]] std::size_t registry::component_count() const noexcept {
  using component_type = std::decay_t<Component>;
  const detail::meta_component_storage_base* storage_base =
      find_component_storage(detail::meta_component_key_of<component_type>());
  if (storage_base == nullptr) {
    return 0;
  }

  return storage_base->size();
}

} // namespace caors::meta

#endif
