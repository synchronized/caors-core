// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#pragma once

#include "caors/meta.h"
#include "caors/traits/type_name.h"
#include "caors/value.h"

#include <catch2/catch_all.hpp>

#include <string_view>

namespace caors::tests::meta_type_basic {

struct sample_type {
  int value = 0;
};

struct anonymous_cpp_type {};

struct guid_sample_type {};

} // namespace caors::tests::meta_type_basic
