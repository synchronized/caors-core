// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

#include <string_view>

using namespace caors::tests::serialization;

TEST_CASE("serialization - string view deserialization can use explicit backing storage",
          "[serialization]") {
  REQUIRE(reflection_result_ok(caors::reflection::class_<codec_string_view_actor>(
                                   "caors.test.serialization.codec_string_view_actor")
                                   .field("name", &codec_string_view_actor::name)
                                   .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<codec_string_view_actor>()));

  codec_string_view_actor actor{std::string_view{"original"}};
  caors::serialization::string_storage storage;
  caors::serialization::context context{};
  context.storage.string_view_storage = &storage;

  const caors::error result = caors::deserialize_json(actor, R"({"name":"Ada"})", context);

  REQUIRE(result.succeeded());
  REQUIRE(storage.size() == 1);
  CHECK(actor.name == "Ada");
}
