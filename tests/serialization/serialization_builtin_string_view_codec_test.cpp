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
  context.string_view_storage = &storage;

  const caors::error result = caors::deserialize_json(actor, R"({"name":"Ada"})", context);

  REQUIRE(result.succeeded());
  REQUIRE(storage.size() == 1);
  CHECK(actor.name == "Ada");
}

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
  context.string_view_storage = &storage;

  const caors::error result =
      caors::deserialize_json(actor, R"({"first":"Ada","second":"Mina"})", context);

  REQUIRE(result.succeeded());
  CHECK(actor.first == "Ada");
  CHECK(actor.second == "Mina");
  CHECK(actor.first.data() != actor.second.data());
  CHECK(storage.size() == 2);
}
