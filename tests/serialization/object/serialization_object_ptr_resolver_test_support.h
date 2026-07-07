// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#pragma once

#include "serialization_test_support.h"

namespace caors::tests::serialization::object_ptr_resolver {

inline void require_object_ptr_actor_metadata() {
  REQUIRE(type_result_ok(caors::meta::type_<caors::object_ptr<object_ptr_target>>(
                             "caors.object_ptr<caors.test.serialization.object_ptr_target>")
                             .result()));
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<object_ptr_actor>("caors.test.serialization.object_ptr_actor")
          .field("target", &object_ptr_actor::target)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<object_ptr_actor>()));
}

} // namespace caors::tests::serialization::object_ptr_resolver
