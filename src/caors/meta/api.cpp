// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta/query.h"
#include "caors/meta/registry.h"
#include "caors/meta/type.h"

#include <string_view>
#include <typeinfo>

namespace caors::meta {

type_id id(core::string_name name) noexcept {
  return id(default_context(), name);
}

type_id id(context& ctx, core::string_name name) noexcept {
  if (!name) {
    return type_id::invalid();
  }

  const type type = ctx.get_registry().find_type(name);
  return type ? type.id() : type_id::invalid();
}

type_id id(std::string_view name) {
  return id(default_context(), name);
}

type_id id(context& ctx, std::string_view name) {
  const type type = ctx.get_registry().find_type(name);
  return type ? type.id() : type_id::invalid();
}

type find(type_id id) { return find(default_context(), id); }

type find(context& ctx, type_id id) { return ctx.get_registry().find_type(id); }

type find(core::type_guid guid) { return find(default_context(), guid); }

type find(context& ctx, core::type_guid guid) { return ctx.get_registry().find_type(guid); }

type find(std::string_view name) { return find(default_context(), name); }

type find(context& ctx, std::string_view name) { return ctx.get_registry().find_type(name); }

type find(core::string_name name) { return find(default_context(), name); }

type find(context& ctx, core::string_name name) { return ctx.get_registry().find_type(name); }

bool is_a(type_id id, type_id base_id) { return is_a(default_context(), id, base_id); }

bool is_a(context& ctx, type_id id, type_id base_id) {
  return ctx.get_registry().is_a(id, base_id);
}

namespace detail {

type_id type_id_of_cpp_type_info(const std::type_info& info) noexcept {
  const type type = get_registry().find_type(info);
  return type ? type.id() : type_id::invalid();
}

} // namespace detail

} // namespace caors::meta
