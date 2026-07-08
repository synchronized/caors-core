// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - generic sequence traits work without codec registration",
          "[serialization]") {
  REQUIRE(reflection_result_ok(caors::reflection::class_<custom_codec_sequence_actor>(
                                   "caors.test.serialization.custom_codec_sequence_actor")
                                   .field("scores", &custom_codec_sequence_actor::scores)
                                   .commit()));
  REQUIRE(
      serialization_result_ok(caors::serialization::register_type<custom_codec_sequence_actor>()));

  custom_codec_sequence_actor actor{};
  actor.scores.values.push_back(4);
  actor.scores.values.push_back(5);

  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"scores":[4,5]})");

  caors::variant::array_type values;
  values.push_back(caors::variant::from(std::int64_t{8}));
  values.push_back(caors::variant::from(std::int64_t{9}));

  caors::variant::object_type members;
  members.push_back(caors::value::variant_member{
      caors::string_name{"scores"},
      0,
      caors::variant::array(std::move(values)),
  });

  custom_codec_sequence_actor decoded{};
  const caors::error result =
      caors::deserialize(decoded, caors::variant::object(std::move(members)));

  REQUIRE(result.succeeded());
  REQUIRE(decoded.scores.values.size() == 2);
  CHECK(decoded.scores.values[0] == 8);
  CHECK(decoded.scores.values[1] == 9);
}
