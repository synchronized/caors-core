// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta/registry.h"
#include "detail/registry_state.h"

#include <vector>

namespace caors::meta {

registry::registry()
    : state_resource_(&memory::library_resource()),
      primary_layer_(std::make_shared<detail::registry_layer_state>(state_resource_)),
      snapshot_(std::make_shared<detail::registry_snapshot>(primary_layer_)),
      state_(&primary_layer_->state()) {}

registry::registry(std::shared_ptr<detail::registry_snapshot> base_snapshot)
    : state_resource_(&memory::library_resource()),
      primary_layer_(std::make_shared<detail::registry_layer_state>(state_resource_)),
      state_(&primary_layer_->state()) {
  core::vector<std::shared_ptr<detail::registry_layer_state>> layers{
      core::vector<std::shared_ptr<detail::registry_layer_state>>::allocator_type{
          &memory::library_resource()}};
  layers.push_back(primary_layer_);
  if (base_snapshot != nullptr) {
    layers.insert(layers.end(), base_snapshot->layers().begin(), base_snapshot->layers().end());
  }
  snapshot_.store(std::make_shared<detail::registry_snapshot>(std::move(layers)),
                  std::memory_order_relaxed);
}

registry::~registry() = default;

registry_layer::registry_layer(std::shared_ptr<detail::registry_snapshot> base_snapshot)
    : registry_(new registry{std::move(base_snapshot)}) {}

registry_layer::~registry_layer() { delete registry_; }

registry& registry_layer::get_registry() noexcept { return *registry_; }

const registry& registry_layer::get_registry() const noexcept { return *registry_; }

bool registry_layer::published() const noexcept { return published_; }

std::unique_ptr<registry_layer> registry::create_layer() const {
  return std::unique_ptr<registry_layer>{new registry_layer{capture_snapshot()}};
}

std::shared_ptr<detail::registry_snapshot> registry::capture_snapshot() const noexcept {
  return snapshot_.load(std::memory_order_acquire);
}

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
    if (state_->frozen) {
      return {error_domain::meta, errc::frozen};
    }
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
    if (state_->frozen) {
      return {error_domain::meta, errc::frozen};
    }
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

error registry::validate() const {
  const auto snapshot = capture_snapshot();
  std::vector<std::unique_lock<std::mutex>> locks{};
  locks.reserve(snapshot->layers().size());
  for (const auto& layer : snapshot->layers()) {
    locks.emplace_back(layer->state().mutex);
  }
  for (const auto& layer : snapshot->layers()) {
    const error status = detail::validate_registry(layer->state(), snapshot.get());
    if (status) {
      return status;
    }
  }
  return {};
}

registry& get_registry() noexcept { return registry::instance(); }

namespace detail {

error register_registration(registration_fn fn) { return get_registry().add_registration(fn); }

} // namespace detail

error run_registrations() { return get_registry().run_registrations(); }

error init_registry() { return get_registry().init(); }

error validate_registry() { return get_registry().validate(); }

} // namespace caors::meta
