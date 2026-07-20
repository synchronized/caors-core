// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta/object.h"

namespace caors::meta {

object::object() noexcept : id_(generate_object_id()) {}

object::~object() = default;

object_id object::id() const noexcept { return id_; }

object_type_id object::type_id() const noexcept { return static_type_id(); }

std::string_view object::type_name() const noexcept { return static_type_name(); }

object_type_id object::static_type_id() noexcept {
#if CAORS_HAS_EXCEPTIONS
  try {
    return caors::meta::find<object>().id();
  } catch (...) {
    return object_type_id::invalid();
  }
#else
  return caors::meta::find<object>().id();
#endif
}

std::string_view object::static_type_name() noexcept { return "caors.object"; }

} // namespace caors::meta
