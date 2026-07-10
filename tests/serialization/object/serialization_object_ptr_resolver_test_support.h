// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#pragma once

#include "serialization_test_support.h"

namespace caors::tests::serialization::object_ptr_resolver {

inline void require_object_ptr_actor_metadata() {
  REQUIRE(reflection_result_ok(
      ((void)caors::meta::type_<object_ptr_actor>("caors.test.serialization.object_ptr_actor"),
       caors::reflection::class_<object_ptr_actor>())
          .field("target", &object_ptr_actor::target)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::class_<object_ptr_actor>().commit()));
}

} // namespace caors::tests::serialization::object_ptr_resolver
