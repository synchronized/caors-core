// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#pragma once

#include "caors/meta.h"
#include "caors/registration.h"

#include <catch2/catch_all.hpp>

namespace caors::tests::meta_type_builder {

struct lazy_meta_builder_type {};

struct fluent_meta_type {};

struct stable_builder_type {};

struct explicit_find_type {};

struct missing_find_type {};

enum class reflected_meta_state {
  idle = 1,
  running = 2,
  hidden = 3,
};

enum class duplicate_meta_state {
  first = 1,
  second = 2,
};

struct reflected_meta_actor {
  int health = 10;

  int add_health(int value) {
    health += value;
    return health;
  }
};

} // namespace caors::tests::meta_type_builder
