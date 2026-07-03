// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/serialization/registry.h"

namespace caors::serialization {

registry& registry::instance() noexcept {
  static registry registry;
  return registry;
}

error registry::register_type(type_id id, type_data data) {
  if (!id) {
    return {error_domain::serialization, errc::invalid_type};
  }

  if (!meta::get_registry().contains(id)) {
    return {error_domain::serialization, errc::missing_type};
  }

  if (!data.valid()) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  return rethrow_as(meta::get_registry().emplace<type_data>(id, data), error_domain::serialization);
}

error registry::register_type(meta::type_view type, type_data data) {
  if (!type.valid()) {
    return {error_domain::serialization, errc::invalid_type};
  }

  return register_type(type.id(), data);
}

bool registry::contains(type_id id) const { return meta::get_registry().has<type_data>(id); }

std::optional<type_data> registry::find_type(type_id id) const {
  return meta::get_registry().find<type_data>(id);
}

std::size_t registry::size() const noexcept { return meta::get_registry().count<type_data>(); }

registry& get_registry() noexcept { return registry::instance(); }

std::optional<type_data> find(type_id id) { return get_registry().find_type(id); }

std::optional<type_data> find(meta::type_view type) {
  return type.valid() ? get_registry().find_type(type.id()) : std::nullopt;
}

bool contains(type_id id) { return get_registry().contains(id); }

bool contains(meta::type_view type) { return type.valid() && contains(type.id()); }

std::size_t size() noexcept { return get_registry().size(); }

namespace detail {

error register_serialization(type_id id, type_data data) {
  return get_registry().register_type(id, data);
}

error register_serialization(meta::type_view type, type_data data) {
  return get_registry().register_type(type, data);
}

} // namespace detail

} // namespace caors::serialization
