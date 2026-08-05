// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/runtime.h"

namespace caors {

runtime::runtime() : runtime(runtime_options{}) {}

runtime::runtime(runtime_options options)
    : meta_registry_(options.meta_registry), meta_context_(meta_registry_) {}

runtime::~runtime() = default;

runtime& runtime::instance() noexcept {
  static runtime value;
  return value;
}

meta::registry& runtime::get_meta_registry() noexcept { return meta_registry_; }

const meta::registry& runtime::get_meta_registry() const noexcept { return meta_registry_; }

meta::context& runtime::get_meta_context() noexcept { return meta_context_; }

const meta::context& runtime::get_meta_context() const noexcept { return meta_context_; }

} // namespace caors
