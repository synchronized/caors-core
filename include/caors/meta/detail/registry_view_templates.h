// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_REGISTRY_VIEW_TEMPLATES_H_
#define CAORS_META_DETAIL_REGISTRY_VIEW_TEMPLATES_H_

#include "caors/meta/component_view.h"
#include "caors/meta/registry.h"

#include <optional>
#include <type_traits>

namespace caors::meta {

template <typename... Components>
template <typename Component>
[[nodiscard]] std::optional<Component> component_view<Components...>::find(type_id id) const {
  if (registry_ == nullptr) {
    return std::nullopt;
  }

  return registry_->template find_component<Component>(id);
}

template <typename... Components>
[[nodiscard]] ::caors::meta::component_view<Components...> registry::component_view() const {
  static_assert(sizeof...(Components) > 0, "Meta view requires at least one component type");

  detail::meta_component_key keys[] = {
      detail::meta_component_key_of<std::decay_t<Components>>()...};
  return ::caors::meta::component_view<Components...>{
      this, view_component_ids(keys, sizeof...(Components))};
}

} // namespace caors::meta

#endif
