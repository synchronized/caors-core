// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/serialization/registry.h"

namespace caors::serialization {

registry& registry::instance() noexcept {
  static registry registry;
  return registry;
}

error registry::attach_schema(type_id id, type_schema data) {
  if (!id) {
    return {error_domain::serialization, errc::invalid_type};
  }

  if (!meta::get_registry().contains(id)) {
    return {error_domain::serialization, errc::missing_type};
  }

  if (!data.valid()) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  return rethrow_as(meta::get_registry().emplace<type_schema>(id, data),
                    error_domain::serialization);
}

error registry::attach_schema(meta::type type, type_schema data) {
  if (!type.registered()) {
    return {error_domain::serialization, errc::invalid_type};
  }

  return attach_schema(type.id(), data);
}

bool registry::contains(type_id id) const { return meta::get_registry().has<type_schema>(id); }

std::optional<type_schema> registry::find_type(type_id id) const {
  return meta::get_registry().find<type_schema>(id);
}

std::size_t registry::size() const noexcept { return meta::get_registry().count<type_schema>(); }

registry& get_registry() noexcept { return registry::instance(); }

std::optional<type_schema> find(type_id id) { return get_registry().find_type(id); }

std::optional<type_schema> find(meta::type type) {
  return type.registered() ? get_registry().find_type(type.id()) : std::nullopt;
}

bool contains(type_id id) { return get_registry().contains(id); }

bool contains(meta::type type) { return type.registered() && contains(type.id()); }

std::size_t size() noexcept { return get_registry().size(); }

} // namespace caors::serialization
