// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#pragma once

#include "caors/meta.h"
#include "caors/registration.h"

#include <catch2/catch_all.hpp>

#include <cstdint>
#include <limits>

namespace caors::tests::meta_type_registry {

struct registry_parent_type {};

struct cpp_sample_type {};

struct guid_parent_type {};

struct error_parent_type {};

struct error_child_type : error_parent_type {};

struct duplicate_guid_type {};

struct duplicate_guid_owner_type {};

struct relation_parent_type {};

struct relation_child_type : relation_parent_type {};

[[nodiscard]] constexpr caors::error meta_error(caors::errc code) noexcept {
  return {caors::error_domain::meta, code};
}

} // namespace caors::tests::meta_type_registry
