// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#pragma once

#include "caors/meta.h"
#include "caors/serialization.h"

#include <catch2/catch_all.hpp>

namespace caors::tests::cross_module {

struct cross_module_actor {
  int id = 0;
  float health = 0.0F;
};

struct cross_module_serialization_first {
  int id = 0;
};

struct cross_module_late_registered {
  int value = 0;
};

[[nodiscard]] inline bool reflection_result_ok(caors::error result) noexcept {
  return result.succeeded() || result.value() == caors::errc::duplicate_field ||
         result.value() == caors::errc::duplicate_method;
}

[[nodiscard]] inline bool serialization_result_ok(caors::error result) noexcept {
  return result.succeeded() || result.value() == caors::errc::duplicate_component;
}

template <typename View> [[nodiscard]] bool view_contains(const View& view, caors::type_id id) {
  for (const caors::type_id candidate : view.ids()) {
    if (candidate == id) {
      return true;
    }
  }

  return false;
}

} // namespace caors::tests::cross_module
