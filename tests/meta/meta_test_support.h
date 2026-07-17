// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#pragma once

#include "caors/meta.h"

#include <catch2/catch_all.hpp>

#include <cstdint>
#include <limits>
#include <string_view>

namespace caors::tests::meta_registry {

struct base_actor_type {};

struct component_actor_type {};

struct missing_component_meta {
  bool constructible = false;
};

struct factory_meta {
  bool constructible = false;
};

struct view_actor_type {};

struct view_player_type : view_actor_type {};

struct view_factory_meta {
  bool constructible = false;
};

struct view_label_meta {
  std::string_view display_name{};
};

[[nodiscard]] constexpr caors::error meta_error(caors::errc code) noexcept {
  return {caors::error_domain::meta, code};
}

} // namespace caors::tests::meta_registry
