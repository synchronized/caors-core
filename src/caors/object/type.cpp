// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/object/type.h"

namespace caors::objects {

type::type(meta::type type) noexcept : type_(type) {}

bool type::valid() const noexcept {
  return type_.registered() && object_factory_capability{type_}.valid();
}

meta::type type::info() const noexcept { return valid() ? type_ : meta::type{}; }

object_type_id type::id() const noexcept {
  return valid() ? type_.id() : object_type_id::invalid();
}

core::string_name type::name() const noexcept {
  return valid() ? type_.name() : core::string_name{};
}

core::type_guid type::guid() const noexcept { return valid() ? type_.guid() : core::type_guid{}; }

std::size_t type::size() const noexcept { return valid() ? type_.size() : 0; }

std::size_t type::alignment() const noexcept { return valid() ? type_.alignment() : 0; }

object_type_id type::parent_id() const noexcept {
  return valid() ? type_.parent_id() : object_type_id::invalid();
}

bool type::constructible() const noexcept { return factory().constructible(); }

object_factory_capability type::factory() const noexcept {
  return valid() ? object_factory_capability{type_} : object_factory_capability{};
}

} // namespace caors::objects
