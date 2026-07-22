// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_REGISTRY_EXTENSION_TEMPLATES_H_
#define CAORS_META_DETAIL_REGISTRY_EXTENSION_TEMPLATES_H_

#include "caors/meta/registry.h"

#include <optional>
#include <type_traits>
#include <utility>

namespace caors::meta::detail {

template <typename Extension, typename... Args>
[[nodiscard]] error emplace_extension(registry& registry, type_id id, Args&&... args) {
  using extension_type = std::decay_t<Extension>;

  if (!id) {
    return {error_domain::meta, errc::invalid_type};
  }

  extension_data data = make_extension_data<extension_type>(
      registry_access::state_resource(registry), std::forward<Args>(args)...);
  return registry_access::add_extension(registry, id, std::move(data));
}

} // namespace caors::meta::detail

namespace caors::meta {

template <typename Extension> [[nodiscard]] bool registry::has_extension(type_id id) const {
  using extension_type = std::decay_t<Extension>;
  return has_extension(id, detail::extension_key_of<extension_type>());
}

template <typename Extension>
[[nodiscard]] std::optional<Extension> registry::find_extension(type_id id) const {
  using extension_type = std::decay_t<Extension>;
  const extension_type* value = find_extension_ptr<extension_type>(id);
  if (value == nullptr) {
    return std::nullopt;
  }

  return *value;
}

template <typename Extension>
[[nodiscard]] const Extension* registry::find_extension_ptr(type_id id) const {
  using extension_type = std::decay_t<Extension>;
  const detail::extension_data* data =
      find_extension_data(id, detail::extension_key_of<extension_type>());
  return data != nullptr ? detail::extension_value<extension_type>(*data) : nullptr;
}

template <typename Extension> [[nodiscard]] std::size_t registry::extension_count() const noexcept {
  using extension_type = std::decay_t<Extension>;
  detail::extension_key keys[] = {detail::extension_key_of<extension_type>()};
  return view_extension_ids(keys, 1).size();
}

} // namespace caors::meta

#endif
