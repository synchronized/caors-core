// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_VALUE_CAPABILITIES_BIND_HELPERS_H_
#define CAORS_META_VALUE_CAPABILITIES_BIND_HELPERS_H_

#include "caors/meta/type.h"
#include "caors/meta/value/any_ref.h"

namespace caors::meta::detail {

[[nodiscard]] inline bool can_bind_capability_source(meta::type expected, any_ref source) noexcept {
  return expected && source.valid() && source.get_type().id() == expected.id();
}

} // namespace caors::meta::detail

#endif
