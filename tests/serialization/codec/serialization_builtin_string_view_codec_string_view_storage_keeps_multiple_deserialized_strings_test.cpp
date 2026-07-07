// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

#include <string_view>

using namespace caors::tests::serialization;

TEST_CASE("serialization - string view storage keeps multiple deserialized strings",
          "[serialization]") {
  REQUIRE(reflection_result_ok(caors::reflection::class_<codec_string_view_pair_actor>(
                                   "caors.test.serialization.codec_string_view_pair_actor")
                                   .field("first", &codec_string_view_pair_actor::first)
                                   .field("second", &codec_string_view_pair_actor::second)
                                   .commit()));
  REQUIRE(
      serialization_result_ok(caors::serialization::register_type<codec_string_view_pair_actor>()));

  codec_string_view_pair_actor actor{};
  caors::serialization::string_storage storage;
  caors::serialization::context context{};
  context.storage.string_view_storage = &storage;

  const caors::error result =
      caors::deserialize_json(context, actor, R"({"first":"Ada","second":"Mina"})");

  REQUIRE(result.succeeded());
  CHECK(actor.first == "Ada");
  CHECK(actor.second == "Mina");
  CHECK(actor.first.data() != actor.second.data());
  CHECK(storage.size() == 2);
}
