// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/serialization.h"

namespace caors {

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

  return rethrow_as(get_meta_registry().emplace<serialization_type_data>(id, data),
                    error_domain::serialization);
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
