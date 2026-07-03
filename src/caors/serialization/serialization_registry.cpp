// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/serialization/registry.h"

namespace caors {

serialization::registry& serialization::registry::instance() noexcept {
  static serialization::registry registry;
  return registry;
}

error serialization::registry::register_type(type_id id, serialization::type_data data) {
  if (!id) {
    return {error_domain::serialization, errc::invalid_type};
  }

  if (!meta::get_registry().contains(id)) {
    return {error_domain::serialization, errc::missing_type};
  }

  if (!data.valid()) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  return rethrow_as(meta::get_registry().emplace<serialization::type_data>(id, data),
                    error_domain::serialization);
}

error serialization::registry::register_type(meta::type_view type, serialization::type_data data) {
  if (!type.valid()) {
    return {error_domain::serialization, errc::invalid_type};
  }

  return register_type(type.id(), data);
}

bool serialization::registry::contains(type_id id) const {
  return meta::get_registry().has<serialization::type_data>(id);
}

std::optional<serialization::type_data> serialization::registry::find_type(type_id id) const {
  return meta::get_registry().find<serialization::type_data>(id);
}

std::size_t serialization::registry::size() const noexcept {
  return meta::get_registry().count<serialization::type_data>();
}

serialization::registry& serialization::get_registry() noexcept {
  return serialization::registry::instance();
}

namespace serialization {

std::optional<serialization::type_data> find(type_id id) {
  return serialization::get_registry().find_type(id);
}

std::optional<serialization::type_data> find(meta::type_view type) {
  return type.valid() ? serialization::find(type.id()) : std::nullopt;
}

bool contains(type_id id) { return serialization::get_registry().contains(id); }

bool contains(meta::type_view type) { return type.valid() && contains(type.id()); }

std::size_t size() noexcept { return serialization::get_registry().size(); }

} // namespace serialization

namespace detail {

error register_serialization(type_id id, serialization::type_data data) {
  return serialization::get_registry().register_type(id, data);
}

error register_serialization(meta::type_view type, serialization::type_data data) {
  return serialization::get_registry().register_type(type, data);
}

} // namespace detail

} // namespace caors
