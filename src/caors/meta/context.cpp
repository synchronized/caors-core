// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta/context.h"

namespace caors::meta {

context::context(registry& registry) noexcept : registry_(&registry) {}

registry& context::get_registry() noexcept { return *registry_; }

const registry& context::get_registry() const noexcept { return *registry_; }

context& default_context() noexcept {
  static context instance{registry::instance()};
  return instance;
}

} // namespace caors::meta
