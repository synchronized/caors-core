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

} // namespace meta

type_id make_type_id(string_name name) noexcept { return meta::id(name); }

type_id make_type_id(std::string_view name) { return meta::id(name); }

std::optional<meta_type> find_type(type_id id) { return meta::find(id); }

std::optional<meta_type> find_type(type_guid guid) { return meta::find(guid); }

std::optional<meta_type> find_type(std::string_view name) { return meta::find(name); }

std::optional<meta_type> find_type(string_name name) { return meta::find(name); }

bool is_a(type_id id, type_id base_id) { return get_meta_registry().is_a(id, base_id); }

} // namespace caors
