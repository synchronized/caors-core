// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：serialization_custom_codec_serialization_custom_codec_specialization_uses_descriptor_capability_test.cpp

#include "serialization_custom_codec_test_support.h"

using namespace caors::tests::serialization;
TEST_CASE("serialization - custom codec specialization uses descriptor capability",
          "[serialization]") {
  const caors::serialization::codec codec =
      caors::serialization::codec_of<policy_custom_codec_value>();
  REQUIRE(codec);
  CHECK(codec.type() == caors::meta::id_of<policy_custom_codec_value>());
  CHECK(codec.can_write());
  CHECK(codec.can_read());
  CHECK(codec.can_make_value());

  const caors::serialization::codec_capability capability =
      caors::meta::resolve<policy_custom_codec_value>()
          .get_capability<caors::serialization::codec_capability>();
  REQUIRE(capability);
  CHECK(capability.type() == caors::meta::id_of<policy_custom_codec_value>());
  CHECK(capability.can_write());
  CHECK(capability.can_read());
  CHECK(capability.can_make_value());

  const caors::serialization::variant_result capability_value = capability.make_value(
      caors::serialization::context{}, caors::any::from(policy_custom_codec_value{5}));
  REQUIRE(capability_value.succeeded());
  REQUIRE(capability_value.value.try_as_i64() != nullptr);
  CHECK(*capability_value.value.try_as_i64() == 105);

  CHECK_FALSE(caors::meta::find<policy_custom_codec_value>());
  CHECK(caors::serialization::find_codec(caors::meta::resolve<policy_custom_codec_value>()));

  require_registered_type<policy_custom_codec_value>(
      "caors.test.serialization.policy_custom_codec_value");
  CHECK(caors::serialization::find_codec(caors::meta::resolve<policy_custom_codec_value>()));
  REQUIRE(reflection_result_ok(((void)caors::meta::class_<policy_custom_codec_actor>(
                                    "caors.test.serialization.policy_custom_codec_actor"),
                                caors::reflection::class_<policy_custom_codec_actor>())
                                   .field("value", &policy_custom_codec_actor::value)
                                   .commit()));
  REQUIRE(
      serialization_result_ok(caors::serialization::class_<policy_custom_codec_actor>().commit()));

  const policy_custom_codec_actor actor{{42}};
  const caors::serialization::serialization_text_result text =
      caors::serialization::serialize_text(actor);

  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"value":142})");

  const caors::serialization::variant_result value =
      caors::serialization::to_variant(caors::any::from(actor.value));

  REQUIRE(value.succeeded());
  REQUIRE(value.value.try_as_i64() != nullptr);
  CHECK(*value.value.try_as_i64() == 142);

  policy_custom_codec_actor decoded{};
  const caors::error result = caors::deserialize(decoded, R"({"value":107})");

  REQUIRE(result.succeeded());
  CHECK(decoded.value.value == 7);
}

TEST_CASE("serialization - descriptor static codec handles erased values without registration",
          "[serialization]") {
  CHECK(caors::serialization::find_codec(caors::meta::resolve<descriptor_static_codec_value>()));

  const descriptor_static_codec_value value{9};
  const caors::serialization::variant_result erased =
      caors::serialization::to_variant(caors::any::from(value));
  REQUIRE(erased.succeeded());
  REQUIRE(erased.value.try_as_i64() != nullptr);
  CHECK(*erased.value.try_as_i64() == 209);

  require_registered_type<descriptor_static_codec_value>(
      "caors.test.serialization.descriptor_static_codec_value");
  REQUIRE(reflection_result_ok(((void)caors::meta::class_<descriptor_static_codec_actor>(
                                    "caors.test.serialization.descriptor_static_codec_actor"),
                                caors::reflection::class_<descriptor_static_codec_actor>())
                                   .field("value", &descriptor_static_codec_actor::value)
                                   .commit()));
  REQUIRE(serialization_result_ok(
      caors::serialization::class_<descriptor_static_codec_actor>().commit()));

  const descriptor_static_codec_actor actor{{11}};
  const caors::serialization::serialization_text_result text =
      caors::serialization::serialize_text(actor);

  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"value":211})");

  descriptor_static_codec_actor decoded{};
  const caors::error result = caors::deserialize(decoded, R"({"value":207})");

  REQUIRE(result.succeeded());
  CHECK(decoded.value.value == 7);
  CHECK(caors::serialization::find_codec(caors::meta::resolve<descriptor_static_codec_value>()));
}

// 原测试文件：serialization_custom_codec_serialization_writer_features_validate_required_field_identifiers_test.cpp

#include "serialization_custom_codec_test_support.h"

using namespace caors::tests::serialization;
TEST_CASE("serialization - writer features validate required field identifiers",
          "[serialization]") {
  REQUIRE(reflection_result_ok(((void)caors::meta::class_<numeric_writer_actor>(
                                    "caors.test.serialization.numeric_writer_actor"),
                                caors::reflection::class_<numeric_writer_actor>())
                                   .field("id", &numeric_writer_actor::id)
                                   .commit()));
  REQUIRE(
      caors::serialization::class_<numeric_writer_actor>({
                                                             caors::serialization::make_field("id"),
                                                         })
          .commit()
          .succeeded());

  numeric_field_writer writer;
  const numeric_writer_actor actor{5};
  const caors::error result = caors::serialize(actor, writer);

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::invalid_field);
}
