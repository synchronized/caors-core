// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#pragma once

#include "caors/meta.h"
#include "caors/registration.h"

#include <catch2/catch_all.hpp>

#include <cstdint>
#include <limits>
#include <string_view>

namespace caors::tests::meta_registry {

struct base_actor_type {};

[[nodiscard]] constexpr caors::error meta_error(caors::errc code) noexcept {
  return {caors::error_domain::meta, code};
}

} // namespace caors::tests::meta_registry
