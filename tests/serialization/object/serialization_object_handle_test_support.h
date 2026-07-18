// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#pragma once

#include "serialization_test_support.h"

#include <string>

namespace caors::tests::serialization::object_handle {

struct object_handle_actor {
  caors::object_handle target{};
  caors::typed_object_handle<object_ptr_target> typed{};
};

inline void require_object_handle_actor_metadata() {
  REQUIRE(registration_result_ok(caors::registration::type_<object_handle_actor>(
                                     "caors.test.serialization.object_handle_actor")
                                     .meta()
                                     .property("target", &object_handle_actor::target)
                                     .property("typed", &object_handle_actor::typed)
                                     .serialization()
                                     .commit()));
}

inline void require_object_handle_target_metadata() {
  const caors::error object_status =
      caors::registration::type_<object_ptr_target>("caors.test.serialization.object_ptr_target")
          .object()
          .commit();
  REQUIRE((object_status == caors::error{} || object_status.value() == caors::errc::duplicate_id));
}

inline void require_handle_value(const caors::variant& value, caors::object_id id,
                                 caors::object_type_id type) {
  REQUIRE(value.is_object());

  const caors::meta::variant_member* id_member = value.find_member(caors::string_name{"id"});
  const caors::meta::variant_member* type_member = value.find_member(caors::string_name{"type"});

  REQUIRE(id_member != nullptr);
  REQUIRE(type_member != nullptr);
  REQUIRE(id_member->value.is_u64());
  REQUIRE(type_member->value.is_u64());
  CHECK(id_member->value.as_u64() == id.value());
  CHECK(type_member->value.as_u64() == type.value());
}

} // namespace caors::tests::serialization::object_handle
