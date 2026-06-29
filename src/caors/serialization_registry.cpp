// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/serialization.h"

namespace caors {
namespace {

[[nodiscard]] error to_serialization_error(meta_component_result result) noexcept {
  switch (result) {
  case meta_component_result::success:
    return {};
  case meta_component_result::invalid_type:
    return {error_domain::serialization, errc::invalid_type};
  case meta_component_result::missing_type:
    return {error_domain::serialization, errc::missing_type};
  case meta_component_result::duplicate_component:
    return {error_domain::serialization, errc::duplicate_component};
  }

  return {error_domain::serialization, errc::invalid_argument};
}

} // namespace

serialization_registry& serialization_registry::instance() noexcept {
  static serialization_registry registry;
  return registry;
}

error serialization_registry::register_type(type_id id, serialization_type_data data) {
  if (!id) {
    return {error_domain::serialization, errc::invalid_type};
  }

  if (!data.valid()) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  return to_serialization_error(get_meta_registry().emplace<serialization_type_data>(id, data));
}

bool serialization_registry::contains(type_id id) const {
  return get_meta_registry().has<serialization_type_data>(id);
}

std::optional<serialization_type_data> serialization_registry::find_type(type_id id) const {
  return get_meta_registry().find<serialization_type_data>(id);
}

std::size_t serialization_registry::size() const noexcept {
  return get_meta_registry().count<serialization_type_data>();
}

serialization_registry& get_serialization_registry() noexcept {
  return serialization_registry::instance();
}

error register_serialization(type_id id, serialization_type_data data) {
  return get_serialization_registry().register_type(id, data);
}

} // namespace caors
