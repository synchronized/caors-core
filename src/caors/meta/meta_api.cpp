// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta.h"

#include <string_view>

namespace caors {

namespace meta {

type_id id(string_name name) noexcept {
  if (!name) {
    return type_id::invalid();
  }

  const std::optional<meta_type> type = get_meta_registry().find_type(name);
  return type.has_value() ? type->id() : type_id::invalid();
}

type_id id(std::string_view name) {
  const std::optional<meta_type> type = get_meta_registry().find_type(name);
  return type.has_value() ? type->id() : type_id::invalid();
}

std::optional<meta_type> find(type_id id) { return get_meta_registry().find_type(id); }

std::optional<meta_type> find(type_guid guid) { return get_meta_registry().find_type(guid); }

std::optional<meta_type> find(std::string_view name) { return get_meta_registry().find_type(name); }

std::optional<meta_type> find(string_name name) { return get_meta_registry().find_type(name); }

bool is_a(type_id id, type_id base_id) { return get_meta_registry().is_a(id, base_id); }

} // namespace meta

} // namespace caors
