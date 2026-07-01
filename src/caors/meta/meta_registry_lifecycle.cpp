// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "meta_internal.h"

namespace caors {

meta_registry::meta_registry() : state_(new meta_registry_state) {}

meta_registry::~meta_registry() { delete state_; }

meta_registry& meta_registry::instance() noexcept {
  static meta_registry registry;
  return registry;
}

meta_registry& get_meta_registry() noexcept { return meta_registry::instance(); }

} // namespace caors
