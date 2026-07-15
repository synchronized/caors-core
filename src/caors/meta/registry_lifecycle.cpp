// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "detail/internal.h"

namespace caors::meta {

registry::registry()
    : state_resource_(&memory::library_resource()),
      state_(memory::make<detail::registry_state>(state_resource_, state_resource_)) {}

registry::~registry() { memory::destroy(state_, state_resource_); }

registry& registry::instance() noexcept {
  static registry instance;
  return instance;
}

error registry::add_registration(registration_fn fn) {
  if (fn == nullptr) {
    return {error_domain::meta, errc::invalid_argument};
  }

  bool run_now = false;
  {
    std::lock_guard lock(state_->mutex);
    for (const registration_fn existing : state_->registrations) {
      if (existing == fn) {
        return {error_domain::meta, errc::already_exists};
      }
    }

    state_->registrations.push_back(fn);
    run_now = state_->registrations_ran;
  }

  return run_now ? fn() : error{};
}

error registry::run_registrations() {
  core::vector<registration_fn> registrations;
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

error registry::init() {
  const error registration_status = run_registrations();
  if (registration_status) {
    return registration_status;
  }

  return validate();
}

bool registry::is_frozen() const noexcept { return false; }

error registry::validate() const {
  std::lock_guard lock(state_->mutex);
  return detail::validate_registry(*state_);
}

registry& get_registry() noexcept { return registry::instance(); }

namespace detail {

error register_registration(registration_fn fn) { return get_registry().add_registration(fn); }

} // namespace detail

error run_registrations() { return get_registry().run_registrations(); }

error init_registry() { return get_registry().init(); }

error validate_registry() { return get_registry().validate(); }

error freeze_registry() { return init_registry(); }

bool is_registry_frozen() noexcept { return get_registry().is_frozen(); }

} // namespace caors::meta
