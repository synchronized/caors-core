// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta.h"

#include <string_view>

namespace caors::meta {

type_id id(core::string_name name) noexcept {
  if (!name) {
    return type_id::invalid();
  }

  const type type = get_registry().find_type(name);
  return type ? type.id() : type_id::invalid();
}

type_id id(std::string_view name) {
  const type type = get_registry().find_type(name);
  return type ? type.id() : type_id::invalid();
}

type find(type_id id) { return get_registry().find_type(id); }

type find(core::type_guid guid) { return get_registry().find_type(guid); }

type find(std::string_view name) { return get_registry().find_type(name); }

type find(core::string_name name) { return get_registry().find_type(name); }

bool is_a(type_id id, type_id base_id) { return get_registry().is_a(id, base_id); }

namespace detail {

type_id type_id_of_cpp_type_info(const std::type_info& info) noexcept {
  const type type = get_registry().find_type(info);
  return type ? type.id() : type_id::invalid();
}

} // namespace detail

} // namespace caors::meta
