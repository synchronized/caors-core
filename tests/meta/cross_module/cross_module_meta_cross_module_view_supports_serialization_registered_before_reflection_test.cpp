// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "cross_module_test_support.h"

using namespace caors::tests::cross_module;
TEST_CASE("meta - cross module view supports serialization registered before reflection",
          "[meta]") {
  const caors::meta::type type = caors::meta::type_<cross_module_serialization_first>(
                                     "caors.test.meta.cross_module_serialization_first")
                                     .get();
  REQUIRE(type.valid());

  REQUIRE(serialization_result_ok(
      caors::serialization::register_type<cross_module_serialization_first>({
          caors::serialization::make_field("id", "actor_id"),
      })));
  REQUIRE(reflection_result_ok(caors::reflection::class_<cross_module_serialization_first>(
                                   "caors.test.meta.cross_module_serialization_first")
                                   .field("id", &cross_module_serialization_first::id)
                                   .commit()));

  auto view = caors::meta::get_registry().view<caors::serialization::type_data>();
  CHECK(view_contains(view, type.id()));

  const cross_module_serialization_first instance{42};
  const caors::serialization::serialization_text_result result = caors::serialize_text(instance);
  REQUIRE(result.succeeded());
  CHECK(result.value == R"({"actor_id":42})");
}
