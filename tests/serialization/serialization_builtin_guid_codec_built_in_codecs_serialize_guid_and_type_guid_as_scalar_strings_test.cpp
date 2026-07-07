// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

#include <optional>
#include <string_view>

using namespace caors::tests::serialization;

TEST_CASE("serialization - built in codecs serialize guid and type guid as scalar strings",
          "[serialization]") {
  const std::optional<caors::guid> id = caors::guid::parse("2f6d1f64-7a3a-4db2-9a2d-123456789abc");
  const std::optional<caors::guid> type =
      caors::guid::parse("5ed39a97-7b20-4d87-86ac-e62a4a615b10");
  REQUIRE(id.has_value());
  REQUIRE(type.has_value());

  REQUIRE(reflection_result_ok(
      caors::reflection::class_<codec_guid_actor>("caors.test.serialization.codec_guid_actor")
          .field("id", &codec_guid_actor::id)
          .field("type", &codec_guid_actor::type)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<codec_guid_actor>()));

  const codec_guid_actor actor{*id, caors::type_guid{*type}};
  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);

  REQUIRE(text.succeeded());
  CHECK(
      text.value ==
      R"({"id":"2f6d1f64-7a3a-4db2-9a2d-123456789abc","type":"5ed39a97-7b20-4d87-86ac-e62a4a615b10"})");

  const caors::serialization::variant_result value = caors::to_serialization_value(actor);
  REQUIRE(value.succeeded());
  REQUIRE(value.value.as_object().size() == 2);
  CHECK(value.value.as_object()[0].value.kind() == caors::value::variant_kind::string);
  CHECK(value.value.as_object()[0].value.as_string() == "2f6d1f64-7a3a-4db2-9a2d-123456789abc");
  CHECK(value.value.as_object()[1].value.kind() == caors::value::variant_kind::string);
  CHECK(value.value.as_object()[1].value.as_string() == "5ed39a97-7b20-4d87-86ac-e62a4a615b10");
}
