// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：cross_module_meta_cross_module_view_supports_serialization_registered_before_meta_fields_test.cpp

#include "cross_module_test_support.h"

#include "caors/meta/detail/type_access.h"

#include <vector>

using namespace caors::tests::cross_module;

TEST_CASE("meta static type data is unique across translation units", "[meta]") {
  const caors::meta::type local = caors::meta::resolve<cross_module_static_type>();
  CHECK(caors::meta::detail::type_access::data_of(local) == static_type_data_from_support_tu());

  const caors::meta::type sequence = caors::meta::resolve<std::vector<int>>();
  CHECK(sequence.get_raw_ops<caors::meta::sequence_ops>() == sequence_ops_from_support_tu());
}
TEST_CASE("meta - cross module view supports unified meta and serialization registration",
          "[meta]") {
  REQUIRE(serialization_result_ok(caors::registration::type_<cross_module_serialization_first>(
                                      "caors.test.meta.cross_module_serialization_first")
                                      .meta()
                                      .property("id", &cross_module_serialization_first::id)
                                      .serialization()
                                      .field("id", "actor_id")
                                      .commit()));

  const caors::meta::type type = caors::meta::find<cross_module_serialization_first>();
  REQUIRE(type.valid());

  auto view = caors::meta::get_registry().extension_view<caors::serialization::type_schema>();
  CHECK(view_contains(view, type.id()));

  const cross_module_serialization_first instance{42};
  const caors::serialization::serialization_text_result result =
      caors::serialization::to_text(instance);
  REQUIRE(result.succeeded());
  CHECK(result.value == R"({"actor_id":42})");
}

// 原测试文件：cross_module_meta_view_finds_types_with_meta_properties_and_serialization_extensions_test.cpp

#include "cross_module_test_support.h"

using namespace caors::tests::cross_module;
TEST_CASE("meta - view finds types with meta properties and serialization extensions", "[meta]") {
  REQUIRE(serialization_result_ok(caors::registration::type_<cross_module_schema_actor>(
                                      "caors.test.meta.cross_module_schema_actor")
                                      .meta()
                                      .property("id", &cross_module_schema_actor::id)
                                      .property("health", &cross_module_schema_actor::health)
                                      .serialization()
                                      .fields({
                                          caors::serialization::make_field("id"),
                                          caors::serialization::make_field("health"),
                                      })
                                      .commit()));

  const auto type = caors::meta::find<cross_module_schema_actor>();
  REQUIRE(type);

  auto view = caors::meta::get_registry().extension_view<caors::serialization::type_schema>();
  CHECK(view_contains(view, type.id()));

  auto meta_type = caors::meta::find(type.id());
  REQUIRE(meta_type);
  CHECK(meta_type.property_count() == 2);

  auto serialization = view.find<caors::serialization::type_schema>(type.id());
  REQUIRE(serialization.has_value());
  CHECK(serialization->fields.size() == 2);
}

// 原测试文件：cross_module_meta_property_handles_remain_valid_after_later_registrations_test.cpp

#include "cross_module_test_support.h"

using namespace caors::tests::cross_module;
TEST_CASE("meta - field handles remain valid after later registrations", "[meta]") {
  REQUIRE(meta_property_result_ok(
      caors::registration::type_<cross_module_actor>("caors.test.meta.cross_module_actor")
          .meta()
          .property("id", &cross_module_actor::id)
          .property("health", &cross_module_actor::health)
          .commit()));

  auto reflected = caors::meta::find("caors.test.meta.cross_module_actor");
  REQUIRE(reflected);

  auto id = reflected.find_property("id");
  REQUIRE(id);
  REQUIRE(id.valid());

  REQUIRE(meta_property_result_ok(caors::registration::type_<cross_module_late_registered>(
                                      "caors.test.meta.cross_module_late_registered")
                                      .meta()
                                      .property("value", &cross_module_late_registered::value)
                                      .commit()));

  const cross_module_actor instance{7, 12.0F};
  const auto value = id.try_get<int>(caors::meta::view(instance));
  REQUIRE(value.has_value());
  CHECK(*value == 7);
  CHECK(id.valid());
}
