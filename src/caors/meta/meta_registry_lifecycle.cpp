// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "meta_internal.h"

namespace caors::meta {

registry::registry() : state_(new registry_state) {}

registry::~registry() { delete state_; }

registry& registry::instance() noexcept {
  static registry instance;
  return instance;
}

error registry::add_registration(registration_fn fn) {
  if (fn == nullptr) {
    return {error_domain::meta, errc::invalid_argument};
  }

  std::lock_guard lock(state_->mutex);
  if (state_->frozen || state_->registrations_ran) {
    return {error_domain::meta, errc::frozen};
  }

  for (const registration_fn existing : state_->registrations) {
    if (existing == fn) {
      return {error_domain::meta, errc::already_exists};
    }
  }

  state_->registrations.push_back(fn);
  return {};
}

error registry::run_registrations() {
  vector<registration_fn> registrations;
  {
    std::lock_guard lock(state_->mutex);
    if (state_->registrations_ran) {
      return {};
    }

    state_->registrations_ran = true;
    registrations = state_->registrations;
  }

  for (const registration_fn fn : registrations) {
    if (fn == nullptr) {
      return {error_domain::meta, errc::invalid_argument};
    }

    const error status = fn();
    if (status) {
      return status;
    }
  }

  return {};
}

error registry::freeze() {
  const error registration_status = run_registrations();
  if (registration_status) {
    return registration_status;
  }

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

bool registry::is_frozen() const noexcept {
  std::lock_guard lock(state_->mutex);
  return state_->frozen;
}

registry& get_registry() noexcept { return registry::instance(); }

namespace detail {

error register_registration(registration_fn fn) { return get_registry().add_registration(fn); }

} // namespace detail

error run_registrations() { return get_registry().run_registrations(); }

error freeze_registry() { return get_registry().freeze(); }

bool is_registry_frozen() noexcept { return get_registry().is_frozen(); }

} // namespace caors::meta
