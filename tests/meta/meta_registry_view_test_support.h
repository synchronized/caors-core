// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#pragma once

#include "meta_property_test_support.h"

namespace test_meta {

[[nodiscard]] inline caors::error register_registry_actor() {
  return ((void)caors::meta::class_<registry_actor>("caors.test.meta.registry_actor")
              .guid(registry_actor_guid),
          caors::meta::class_<registry_actor>())
      .property("id", &registry_actor::id)
      .property("health", &registry_actor::health)
      .commit();
}

[[nodiscard]] inline caors::error register_view_actor() {
  return ((void)caors::meta::class_<view_actor>("caors.test.meta.view_actor").guid(view_actor_guid),
          caors::meta::class_<view_actor>())
      .property("id", &view_actor::id)
      .property("health", &view_actor::health)
      .commit();
}

} // namespace test_meta
