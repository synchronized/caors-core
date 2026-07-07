// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_custom_codec_test_support.h"

using namespace caors::tests::serialization;
TEST_CASE("serialization - custom codec specialization registers one runtime table",
          "[serialization]") {
  require_registered_type<policy_custom_codec_value>(
      "caors.test.serialization.policy_custom_codec_value");
  REQUIRE(reflection_result_ok(caors::reflection::class_<policy_custom_codec_actor>(
                                   "caors.test.serialization.policy_custom_codec_actor")
                                   .field("value", &policy_custom_codec_actor::value)
                                   .commit()));
  REQUIRE(
      serialization_result_ok(caors::serialization::register_codec<policy_custom_codec_value>()));
  REQUIRE(
      serialization_result_ok(caors::serialization::register_type<policy_custom_codec_actor>()));

  const policy_custom_codec_actor actor{{42}};
  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);

  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"value":142})");

  const caors::serialization::variant_result value =
      caors::serialization::to_variant(caors::any::from(actor.value));

  REQUIRE(value.succeeded());
  REQUIRE(value.value.try_as_i64() != nullptr);
  CHECK(*value.value.try_as_i64() == 142);

  policy_custom_codec_actor decoded{};
  const caors::error result = caors::deserialize_json(decoded, R"({"value":107})");

  REQUIRE(result.succeeded());
  CHECK(decoded.value.value == 7);
}
