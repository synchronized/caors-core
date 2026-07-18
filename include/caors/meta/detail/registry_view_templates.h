// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_REGISTRY_VIEW_TEMPLATES_H_
#define CAORS_META_DETAIL_REGISTRY_VIEW_TEMPLATES_H_

#include "caors/meta/extension_view.h"
#include "caors/meta/registry.h"

#include <optional>
#include <type_traits>

namespace caors::meta {

template <typename... Extensions>
template <typename Component>
[[nodiscard]] std::optional<Component> extension_view<Extensions...>::find(type_id id) const {
  if (registry_ == nullptr) {
    return std::nullopt;
  }

  return registry_->template find_extension<Component>(id);
}

template <typename... Extensions>
[[nodiscard]] ::caors::meta::extension_view<Extensions...> registry::extension_view() const {
  static_assert(sizeof...(Extensions) > 0, "Meta view requires at least one extension type");

  detail::extension_key keys[] = {detail::extension_key_of<std::decay_t<Extensions>>()...};
  return ::caors::meta::extension_view<Extensions...>{
      this, view_extension_ids(keys, sizeof...(Extensions))};
}

} // namespace caors::meta

#endif
