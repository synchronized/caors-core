// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#pragma once

#include "caors/meta.h"

#include <catch2/catch_all.hpp>

namespace caors::tests::meta_type_builder {

struct lazy_meta_builder_type {};

struct fluent_meta_type {};

struct stable_builder_type {};

struct explicit_find_type {};

struct missing_find_type {};

struct reflected_meta_actor {
  int health = 10;

  int add_health(int value) {
    health += value;
    return health;
  }
};

} // namespace caors::tests::meta_type_builder
