// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#pragma once

#include "caors/core/containers.h"
#include "caors/memory.h"
#include "caors/meta.h"
#include "caors/value.h"

#include <catch2/catch_all.hpp>

namespace caors::tests::meta_type_factory {

struct default_factory_type {
  int value = 11;
};

struct missing_default_factory_type {
  explicit missing_default_factory_type(int input) : value(input) {}

  int value = 0;
};

struct custom_factory_type {
  explicit custom_factory_type(int input) : value(input) {}

  int value = 0;
};

[[nodiscard]] constexpr caors::error meta_error(caors::errc code) noexcept {
  return {caors::error_domain::meta, code};
}

inline void* construct_custom_factory_type(caors::memory_resource* resource) {
  if (resource != nullptr) {
    return caors::memory::make<custom_factory_type>(resource, 23);
  }

  return new custom_factory_type(23);
}

inline void destroy_custom_factory_type(caors::memory_resource* resource, void* value) noexcept {
  auto* typed = static_cast<custom_factory_type*>(value);
  if (resource != nullptr) {
    caors::memory::destroy(typed, resource);
    return;
  }

  delete typed;
}

} // namespace caors::tests::meta_type_factory
