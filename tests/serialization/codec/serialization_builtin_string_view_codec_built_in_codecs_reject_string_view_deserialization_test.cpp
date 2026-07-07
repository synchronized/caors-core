// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

#include <string_view>

using namespace caors::tests::serialization;

TEST_CASE("serialization - built in codecs reject string view deserialization", "[serialization]") {
  REQUIRE(reflection_result_ok(caors::reflection::class_<codec_string_view_actor>(
                                   "caors.test.serialization.codec_string_view_actor")
                                   .field("name", &codec_string_view_actor::name)
                                   .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<codec_string_view_actor>()));

  codec_string_view_actor actor{std::string_view{"original"}};
  const caors::error result = caors::deserialize_json(actor, R"({"name":"Ada"})");

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::unsupported);
  CHECK(actor.name == "original");
}
