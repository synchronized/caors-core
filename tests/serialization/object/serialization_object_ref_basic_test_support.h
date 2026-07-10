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
  const caors::error object_status = caors::objects::class_<object_ref_target>().commit();
  REQUIRE((object_status == caors::error{} || object_status.value() == caors::errc::duplicate_id));

  REQUIRE(reflection_result_ok(
      caors::reflection::class_<object_ref_target>(object_ref_target::object_type_name)
          .field("value", &object_ref_target::value)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::class_<object_ref_target>().commit()));
}

inline void require_object_ref_owner_actor_metadata() {
  require_object_ref_target_metadata();

  REQUIRE(reflection_result_ok(caors::reflection::class_<object_ref_owner_actor>(
                                   "caors.test.serialization.object_ref_owner_actor")
                                   .field("owner", &object_ref_owner_actor::owner)
                                   .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::class_<object_ref_owner_actor>().commit()));
}

} // namespace caors::tests::serialization::object_ref_basic
