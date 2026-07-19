// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#pragma once

#include "caors/meta.h"
#include "caors/meta/value.h"
#include "caors/registration.h"
#include "caors/traits/type_name.h"

#include <catch2/catch_all.hpp>

#include <string_view>

namespace caors::tests::meta_type_basic {

struct sample_type {
  int value = 0;
};

struct anonymous_cpp_type {};

struct guid_sample_type {};

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
