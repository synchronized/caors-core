// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#pragma once

#include "serialization_test_support.h"

#include <string>
#include <string_view>

namespace caors::tests::serialization::object_ptr_manager {

inline void require_object_ptr_actor_metadata() {
  const caors::error object_status =
      caors::registration::type_<object_ptr_target>(
          "caors.test.serialization.object_ptr_target")
          .object()
          .commit();
  REQUIRE((object_status == caors::error{} || object_status.value() == caors::errc::duplicate_id));
  REQUIRE(reflection_result_ok(
      ((void)caors::meta::class_<object_ptr_actor>("caors.test.serialization.object_ptr_actor"),
       caors::meta::class_<object_ptr_actor>())
          .property("target", &object_ptr_actor::target)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::class_<object_ptr_actor>().commit()));
}

} // namespace caors::tests::serialization::object_ptr_manager
