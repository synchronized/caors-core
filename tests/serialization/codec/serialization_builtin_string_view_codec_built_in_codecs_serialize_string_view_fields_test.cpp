// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

#include <string_view>

using namespace caors::tests::serialization;

TEST_CASE("serialization - built in codecs serialize string view fields", "[serialization]") {
  REQUIRE(reflection_result_ok(caors::reflection::class_<codec_string_view_actor>(
                                   "caors.test.serialization.codec_string_view_actor")
                                   .field("name", &codec_string_view_actor::name)
                                   .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<codec_string_view_actor>()));

  const codec_string_view_actor actor{std::string_view{"Ada"}};
  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);

  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"name":"Ada"})");
}
