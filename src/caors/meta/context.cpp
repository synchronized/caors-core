// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta/context.h"

#include "caors/runtime.h"

namespace caors::meta {

context::context(registry& registry) noexcept : registry_(&registry) {}

registry& context::get_registry() noexcept { return *registry_; }

const registry& context::get_registry() const noexcept { return *registry_; }

context& default_context() noexcept { return runtime::instance().get_meta_context(); }

} // namespace caors::meta
