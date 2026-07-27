// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/serialization/schema_query.h"

#include "caors/meta/context.h"
#include "caors/meta/static_capability.h"

namespace caors::serialization {

schema_view find_schema(meta::type type) noexcept {
  return type.registered() ? type.get_capability<schema_capability>() : schema_view{};
}

bool contains(meta::type type) { return find_schema(type).valid(); }

std::size_t size(meta::context& context) noexcept {
  return meta::detail::capability_count(context.get_registry(),
                                        meta::capability_key_of<schema_capability>());
}

std::size_t size() noexcept { return size(meta::default_context()); }

} // namespace caors::serialization
