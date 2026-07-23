// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#pragma once

#include "caors/meta.h"
#include "caors/registration.h"
#include "caors/serialization.h"

#include <catch2/catch_all.hpp>

namespace caors::tests::cross_module {

struct cross_module_actor {
  int id = 0;
  float health = 0.0F;
};

struct cross_module_schema_actor {
  int id = 0;
  float health = 0.0F;
};

struct cross_module_serialization_first {
  int id = 0;
};

struct cross_module_late_registered {
  int value = 0;
};

struct cross_module_static_type {
  int value = 0;
};

[[nodiscard]] const void* static_type_data_from_support_tu() noexcept;
[[nodiscard]] const void* sequence_ops_from_support_tu() noexcept;
[[nodiscard]] const void* codec_ops_from_support_tu() noexcept;

[[nodiscard]] inline bool meta_property_result_ok(caors::error result) noexcept {
  return result.succeeded() || result.value() == caors::errc::duplicate_field ||
         result.value() == caors::errc::duplicate_method;
}

[[nodiscard]] inline bool serialization_result_ok(caors::error result) noexcept {
  return result.succeeded() || result.value() == caors::errc::duplicate_component ||
         result.value() == caors::errc::duplicate_id;
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
