// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：cross_module_meta_cross_module_view_supports_serialization_registered_before_reflection_test.cpp

#include "cross_module_test_support.h"

using namespace caors::tests::cross_module;
TEST_CASE("meta - cross module view supports serialization registered before reflection",
          "[meta]") {
  const caors::meta::type type = caors::meta::class_<cross_module_serialization_first>(
                                     "caors.test.meta.cross_module_serialization_first")
                                     .get();
  REQUIRE(type.valid());

  REQUIRE(serialization_result_ok(caors::serialization::class_<cross_module_serialization_first>(
                                      {
                                          caors::serialization::make_field("id", "actor_id"),
                                      })
                                      .commit()));
  REQUIRE(reflection_result_ok(((void)caors::meta::class_<cross_module_serialization_first>(
                                    "caors.test.meta.cross_module_serialization_first"),
                                caors::reflection::class_<cross_module_serialization_first>())
                                   .field("id", &cross_module_serialization_first::id)
                                   .commit()));

  auto view = caors::meta::get_registry().component_view<caors::serialization::type_schema>();
  CHECK(view_contains(view, type.id()));

  const cross_module_serialization_first instance{42};
  const caors::serialization::serialization_text_result result =
      caors::serialization::serialize_text(instance);
  REQUIRE(result.succeeded());
  CHECK(result.value == R"({"actor_id":42})");
}

// 原测试文件：cross_module_meta_view_finds_types_with_reflection_and_serialization_components_test.cpp

#include "cross_module_test_support.h"

using namespace caors::tests::cross_module;
TEST_CASE("meta - view finds types with reflection and serialization components", "[meta]") {
  REQUIRE(reflection_result_ok(
      ((void)caors::meta::class_<cross_module_actor>("caors.test.meta.cross_module_actor"),
       caors::reflection::class_<cross_module_actor>())
          .field("id", &cross_module_actor::id)
          .field("health", &cross_module_actor::health)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::class_<cross_module_actor>(
                                      {
                                          caors::serialization::make_field("id"),
                                          caors::serialization::make_field("health"),
                                      })
                                      .commit()));

  const auto type = caors::meta::find<cross_module_actor>();
  REQUIRE(type);

  auto view = caors::meta::get_registry().component_view<caors::serialization::type_schema>();
  CHECK(view_contains(view, type.id()));

  auto reflection = caors::reflection::find(type.id());
  REQUIRE(reflection);
  CHECK(reflection.field_count() == 2);

  auto serialization = view.find<caors::serialization::type_schema>(type.id());
  REQUIRE(serialization.has_value());
  CHECK(serialization->fields.size() == 2);
}

// 原测试文件：cross_module_reflection_field_handles_remain_valid_after_later_registrations_test.cpp

#include "cross_module_test_support.h"

using namespace caors::tests::cross_module;
TEST_CASE("reflection - field handles remain valid after later registrations", "[reflection]") {
  REQUIRE(reflection_result_ok(
      ((void)caors::meta::class_<cross_module_actor>("caors.test.meta.cross_module_actor"),
       caors::reflection::class_<cross_module_actor>())
          .field("id", &cross_module_actor::id)
          .field("health", &cross_module_actor::health)
          .commit()));

  auto reflected = caors::reflection::find("caors.test.meta.cross_module_actor");
  REQUIRE(reflected);

  auto id = reflected.find_field("id");
  REQUIRE(id);
  REQUIRE(id.valid());

  REQUIRE(reflection_result_ok(((void)caors::meta::class_<cross_module_late_registered>(
                                    "caors.test.meta.cross_module_late_registered"),
                                caors::reflection::class_<cross_module_late_registered>())
                                   .field("value", &cross_module_late_registered::value)
                                   .commit()));

  const cross_module_actor instance{7, 12.0F};
  const auto value = id.try_get<int>(&instance);
  REQUIRE(value.has_value());
  CHECK(*value == 7);
  CHECK(id.valid());
}
