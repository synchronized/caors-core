// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/serialization/registry.h"

namespace caors::serialization {

registry& registry::instance() noexcept {
  static registry registry;
  return registry;
}

bool registry::contains(type_id id) const {
  return meta::get_registry().has_extension<type_schema>(id);
}

std::optional<type_schema> registry::find_type(type_id id) const {
  return meta::get_registry().find_extension<type_schema>(id);
}

std::size_t registry::size() const noexcept {
  return meta::get_registry().extension_count<type_schema>();
}

registry& get_registry() noexcept { return registry::instance(); }

std::optional<type_schema> find(type_id id) { return get_registry().find_type(id); }

std::optional<type_schema> find(meta::type type) {
  return type.registered() ? get_registry().find_type(type.id()) : std::nullopt;
}

schema_view find_schema(meta::type type) noexcept {
  return type.registered() ? type.get_capability<schema_view>() : schema_view{};
}

bool contains(type_id id) { return get_registry().contains(id); }

bool contains(meta::type type) { return find_schema(type).valid(); }

std::size_t size() noexcept { return get_registry().size(); }

} // namespace caors::serialization
