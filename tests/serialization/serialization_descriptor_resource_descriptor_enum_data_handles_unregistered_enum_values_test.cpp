// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_descriptor_resource_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::descriptor_resource;

TEST_CASE("serialization - descriptor enum data handles unregistered enum values",
          "[serialization]") {
  descriptor_enum_color color = descriptor_enum_color::blue;

  const caors::serialization::variant_result ref_value =
      caors::serialization::to_variant(caors::any_ref::from(color));
  REQUIRE(ref_value.succeeded());
  REQUIRE(ref_value.value.try_as_u64() != nullptr);
  CHECK(ref_value.value.as_u64() == 2U);

  const caors::serialization::variant_result owned_value =
      caors::serialization::to_variant(caors::any::from(color));
  REQUIRE(owned_value.succeeded());
  REQUIRE(owned_value.value.try_as_u64() != nullptr);
  CHECK(owned_value.value.as_u64() == 2U);

  REQUIRE(reflection_result_ok(caors::reflection::class_<descriptor_enum_actor>(
                                   "caors.test.serialization.descriptor_enum_actor")
                                   .field("colors", &descriptor_enum_actor::colors)
                                   .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<descriptor_enum_actor>()));

  const descriptor_enum_actor actor{{descriptor_enum_color::blue, descriptor_enum_color::red}};
  const caors::serialization::serialization_text_result text = caors::serialize_json(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"colors":[2,1]})");

  descriptor_enum_actor decoded{};
  const caors::error result = caors::deserialize_json(decoded, R"({"colors":[1,2]})");
  REQUIRE(result.succeeded());
  REQUIRE(decoded.colors.size() == 2);
  CHECK(decoded.colors[0] == descriptor_enum_color::red);
  CHECK(decoded.colors[1] == descriptor_enum_color::blue);
}
