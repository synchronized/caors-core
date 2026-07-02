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

error meta_registry::add_registration(meta_registration_fn fn) {
  if (fn == nullptr) {
    return {error_domain::meta, errc::invalid_argument};
  }

  std::lock_guard lock(state_->mutex);
  if (state_->frozen || state_->registrations_ran) {
    return {error_domain::meta, errc::frozen};
  }

  for (const meta_registration_fn existing : state_->registrations) {
    if (existing == fn) {
      return {error_domain::meta, errc::already_exists};
    }
  }

  state_->registrations.push_back(fn);
  return {};
}

error meta_registry::run_registrations() {
  vector<meta_registration_fn> registrations;
  {
    std::lock_guard lock(state_->mutex);
    if (state_->registrations_ran) {
      return {};
    }

    state_->registrations_ran = true;
    registrations = state_->registrations;
  }

  for (const meta_registration_fn fn : registrations) {
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

error meta_registry::freeze() {
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

bool meta_registry::is_frozen() const noexcept {
  std::lock_guard lock(state_->mutex);
  return state_->frozen;
}

meta_registry& get_meta_registry() noexcept { return meta_registry::instance(); }

namespace detail {

error register_meta_registration(meta_registration_fn fn) {
  return get_meta_registry().add_registration(fn);
}

} // namespace detail

error run_meta_registrations() { return get_meta_registry().run_registrations(); }

error freeze_meta_registry() { return get_meta_registry().freeze(); }

bool is_meta_registry_frozen() noexcept { return get_meta_registry().is_frozen(); }

} // namespace caors
