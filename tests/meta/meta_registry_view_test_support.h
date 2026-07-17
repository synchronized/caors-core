// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#pragma once

#include "meta_field_test_support.h"

namespace test_meta {

[[nodiscard]] inline caors::error register_registry_actor() {
  return ((void)caors::meta::class_<registry_actor>("caors.test.meta.registry_actor")
              .guid(registry_actor_guid),
          caors::meta::class_<registry_actor>())
      .field("id", &registry_actor::id)
      .field("health", &registry_actor::health)
      .commit();
}

[[nodiscard]] inline caors::error register_view_actor() {
  return ((void)caors::meta::class_<view_actor>("caors.test.meta.view_actor").guid(view_actor_guid),
          caors::meta::class_<view_actor>())
      .field("id", &view_actor::id)
      .field("health", &view_actor::health)
      .commit();
}

} // namespace test_meta
