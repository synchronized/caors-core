// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta.h"

#include <string_view>

namespace caors::meta {

type_id id(core::string_name name) noexcept {
  if (!name) {
    return type_id::invalid();
  }

  const type_view type = get_registry().find_type(name);
  return type ? type.id() : type_id::invalid();
}

type_id id(std::string_view name) {
  const type_view type = get_registry().find_type(name);
  return type ? type.id() : type_id::invalid();
}

type_view find(type_id id) { return get_registry().find_type(id); }

type_view find(core::type_guid guid) { return get_registry().find_type(guid); }

type_view find(std::string_view name) { return get_registry().find_type(name); }

type_view find(core::string_name name) { return get_registry().find_type(name); }

bool is_a(type_id id, type_id base_id) { return get_registry().is_a(id, base_id); }

} // namespace caors::meta
