// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "cross_module_test_support.h"

using namespace caors::tests::cross_module;
TEST_CASE("meta - view finds types with reflection and serialization components", "[meta]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<cross_module_actor>("caors.test.meta.cross_module_actor")
          .field("id", &cross_module_actor::id)
          .field("health", &cross_module_actor::health)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<cross_module_actor>({
      caors::serialization::make_field("id"),
      caors::serialization::make_field("health"),
  })));

  const auto type = caors::meta::find<cross_module_actor>();
  REQUIRE(type);

  auto view = caors::meta::get_registry().view<caors::serialization::type_data>();
  CHECK(view_contains(view, type.id()));

  auto reflection = caors::reflection::find(type.id());
  REQUIRE(reflection);
  CHECK(reflection.field_count() == 2);

  auto serialization = view.find<caors::serialization::type_data>(type.id());
  REQUIRE(serialization.has_value());
  CHECK(serialization->fields.size() == 2);
}
