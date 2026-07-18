// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#pragma once

#include "serialization_test_support.h"

#include "caors/object.h"

#include <string_view>

namespace caors::tests::serialization::object_ref_basic {

class object_ref_target : public caors::object_base<object_ref_target>,
                          public caors::objects::ref_counted {
public:
  static constexpr std::string_view object_type_name = "caors.test.serialization.object_ref_target";

  int value = 0;
};

struct object_ref_owner_actor {
  caors::object_ref<object_ref_target> owner{};
};

inline void require_object_ref_target_metadata() {
  REQUIRE(registration_result_ok(caors::registration::type_<object_ref_target>()
                                     .meta()
                                     .property("value", &object_ref_target::value)
                                     .serialization()
                                     .commit()));
}

inline void require_object_ref_owner_actor_metadata() {
  require_object_ref_target_metadata();

  require_serializable_property_type<object_ref_owner_actor>(
      "caors.test.serialization.object_ref_owner_actor", "owner", &object_ref_owner_actor::owner);
}

} // namespace caors::tests::serialization::object_ref_basic
