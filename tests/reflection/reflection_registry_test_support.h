// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#pragma once

#include "reflection_test_support.h"

namespace test_reflection {

[[nodiscard]] inline caors::error register_registry_actor() {
  return ((void)caors::meta::type_<registry_actor>("caors.test.reflection.registry_actor")
              .guid(registry_actor_guid),
          caors::reflection::class_<registry_actor>())
      .field("id", &registry_actor::id)
      .field("health", &registry_actor::health)
      .commit();
}

[[nodiscard]] inline caors::error register_view_actor() {
  return ((void)caors::meta::type_<view_actor>("caors.test.reflection.view_actor")
              .guid(view_actor_guid),
          caors::reflection::class_<view_actor>())
      .field("id", &view_actor::id)
      .field("health", &view_actor::health)
      .commit();
}

} // namespace test_reflection
