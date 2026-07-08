// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#pragma once

#include "serialization_test_support.h"

#include <string>
#include <string_view>

namespace caors::tests::serialization::object_ptr_graph {

struct object_ptr_graph_actor {
  caors::object_ptr<object_ptr_target> primary{};
  caors::object_ptr<object_ptr_target> secondary{};
  caors::vector<caors::object_ptr<object_ptr_target>> related{};
};

inline void require_object_ptr_target_class_metadata() {
  const caors::error object_status =
      caors::objects::class_<object_ptr_target>("caors.test.serialization.object_ptr_target")
          .commit();
  REQUIRE((object_status == caors::error{} || object_status.value() == caors::errc::duplicate_id));
}

inline void require_object_ptr_graph_metadata() {
  REQUIRE(reflection_result_ok(caors::reflection::class_<object_ptr_graph_actor>(
                                   "caors.test.serialization.object_ptr_graph_actor")
                                   .field("primary", &object_ptr_graph_actor::primary)
                                   .field("secondary", &object_ptr_graph_actor::secondary)
                                   .field("related", &object_ptr_graph_actor::related)
                                   .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<object_ptr_graph_actor>()));
}

} // namespace caors::tests::serialization::object_ptr_graph
