// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：serialization_custom_codec_serialization_custom_codec_specialization_registers_one_runtime_table_test.cpp

#include "serialization_custom_codec_test_support.h"

using namespace caors::tests::serialization;
TEST_CASE("serialization - custom codec specialization registers one runtime table",
          "[serialization]") {
  const caors::serialization::codec_view codec =
      caors::serialization::codec_of<policy_custom_codec_value>();
  REQUIRE(codec);
  CHECK(codec.type() == caors::meta::id_of<policy_custom_codec_value>());
  CHECK(codec.can_write());
  CHECK(codec.can_read());
  CHECK(codec.can_make_value());
  CHECK_FALSE(caors::meta::find<policy_custom_codec_value>().registered());
  REQUIRE(
      serialization_result_ok(caors::serialization::register_codec<policy_custom_codec_value>()));
  CHECK(caors::serialization::find_codec(caors::meta::type_of<policy_custom_codec_value>()));

  require_registered_type<policy_custom_codec_value>(
      "caors.test.serialization.policy_custom_codec_value");
  REQUIRE(reflection_result_ok(caors::reflection::class_<policy_custom_codec_actor>(
                                   "caors.test.serialization.policy_custom_codec_actor")
                                   .field("value", &policy_custom_codec_actor::value)
                                   .commit()));
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

// 原测试文件：serialization_custom_codec_serialization_writer_features_validate_required_field_identifiers_test.cpp

#include "serialization_custom_codec_test_support.h"

using namespace caors::tests::serialization;
TEST_CASE("serialization - writer features validate required field identifiers",
          "[serialization]") {
  REQUIRE(reflection_result_ok(caors::reflection::class_<numeric_writer_actor>(
                                   "caors.test.serialization.numeric_writer_actor")
                                   .field("id", &numeric_writer_actor::id)
                                   .commit()));
  REQUIRE(caors::serialization::register_type<numeric_writer_actor>(
              {
                  caors::serialization::make_field("id"),
              })
              .succeeded());

  numeric_field_writer writer;
  const numeric_writer_actor actor{5};
  const caors::error result = caors::serialize(actor, writer);

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::invalid_field);
}
