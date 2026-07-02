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

error meta_registry::freeze() {
  std::lock_guard lock(state_->mutex);
  if (state_->frozen) {
    return {};
  }

  const error status = detail::validate_registry_for_freeze(*state_);
  if (status) {
    return status;
  }

  state_->frozen = true;
  return {};
}

bool meta_registry::is_frozen() const noexcept {
  std::lock_guard lock(state_->mutex);
  return state_->frozen;
}

meta_registry& get_meta_registry() noexcept { return meta_registry::instance(); }

error freeze_meta_registry() { return get_meta_registry().freeze(); }

bool is_meta_registry_frozen() noexcept { return get_meta_registry().is_frozen(); }

} // namespace caors
