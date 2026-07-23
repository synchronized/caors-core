// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#pragma once

#include "caors/meta.h"
#include "caors/meta/value.h"
#include "caors/registration.h"
#include "caors/traits/type_identity.h"
#include "caors/traits/type_name.h"

#include <catch2/catch_all.hpp>

#include <string_view>

namespace caors::tests::meta_type_basic {

struct sample_type {
  int value = 0;
};

struct anonymous_cpp_type {};

struct guid_sample_type {};

struct declared_identity_type {};
inline constexpr type_guid declared_identity_guid{
    guid::from_bytes({0x18, 0x2A, 0x3C, 0x4E, 0x50, 0x62, 0x74, 0x86, 0x98, 0xAA, 0xBC, 0xCE, 0xD0,
                      0xE2, 0xF4, 0x06})};

struct nested_inner_type {};

enum class nested_kind_type {
  first,
  second,
};

struct nested_owner_type {};

struct nested_duplicate_name_owner_type {};

struct nested_duplicate_id_owner_type {};

struct nested_missing_owner_type {};

struct nested_duplicate_name_type {};

struct nested_duplicate_id_type {};

struct nested_missing_type {};

} // namespace caors::tests::meta_type_basic

namespace caors::traits {

template <> struct type_identity<tests::meta_type_basic::declared_identity_type> {
  [[nodiscard]] static constexpr std::string_view name() noexcept {
    return "caors.test.type.declared_identity";
  }

  [[nodiscard]] static constexpr core::type_guid guid() noexcept {
    return tests::meta_type_basic::declared_identity_guid;
  }
};

} // namespace caors::traits
