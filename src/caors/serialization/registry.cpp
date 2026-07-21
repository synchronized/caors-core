// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/serialization/registry.h"

namespace caors::serialization {

schema_view find_schema(meta::type type) noexcept {
  return type.registered() ? type.get_capability<schema_view>() : schema_view{};
}

bool contains(meta::type type) { return find_schema(type).valid(); }

std::size_t size() noexcept { return meta::get_registry().extension_count<type_schema>(); }

} // namespace caors::serialization
