// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#pragma once

#include "serialization_test_support.h"

#include <string>
#include <string_view>

namespace caors::tests::serialization::object_ptr_manager {

inline void require_object_ptr_actor_metadata() {
  const caors::error object_status =
      caors::objects::class_<object_ptr_target>("caors.test.serialization.object_ptr_target")
          .commit();
  REQUIRE((object_status == caors::error{} || object_status.value() == caors::errc::duplicate_id));
  REQUIRE(type_result_ok(caors::meta::type_<caors::object_ptr<object_ptr_target>>(
                             "caors.object_ptr<caors.test.serialization.object_ptr_target>")
                             .result()));
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<object_ptr_actor>("caors.test.serialization.object_ptr_actor")
          .field("target", &object_ptr_actor::target)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<object_ptr_actor>()));
}

} // namespace caors::tests::serialization::object_ptr_manager
