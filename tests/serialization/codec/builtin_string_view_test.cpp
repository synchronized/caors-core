// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：serialization_builtin_string_view_codec_built_in_codecs_reject_string_view_deserialization_test.cpp

#include "serialization_test_support.h"

#include <string_view>

using namespace caors::tests::serialization;

namespace {

void require_codec_string_view_actor_metadata() {
  require_serializable_property_type<codec_string_view_actor>(
      "caors.test.serialization.codec_string_view_actor", "name", &codec_string_view_actor::name);
}

} // namespace

TEST_CASE("serialization - built in codecs reject string view deserialization", "[serialization]") {
  require_codec_string_view_actor_metadata();

  codec_string_view_actor actor{std::string_view{"original"}};
  const caors::error result = caors::read_json_from(actor, R"({"name":"Ada"})");

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::unsupported);
  CHECK(actor.name == "original");
}

// 原测试文件：serialization_builtin_string_view_codec_built_in_codecs_serialize_string_view_fields_test.cpp

#include "serialization_test_support.h"

#include <string_view>

using namespace caors::tests::serialization;

TEST_CASE("serialization - built in codecs serialize string view fields", "[serialization]") {
  require_codec_string_view_actor_metadata();

  const codec_string_view_actor actor{std::string_view{"Ada"}};
  const caors::serialization::serialization_text_result text = caors::serialization::to_text(actor);

  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"name":"Ada"})");
}

// 原测试文件：serialization_builtin_string_view_codec_string_view_deserialization_can_use_explicit_backing_storage_test.cpp

#include "serialization_test_support.h"

#include <string_view>

using namespace caors::tests::serialization;

TEST_CASE("serialization - string view deserialization can use explicit backing storage",
          "[serialization]") {
  require_codec_string_view_actor_metadata();

  codec_string_view_actor actor{std::string_view{"original"}};
  caors::serialization::string_storage storage;
  caors::serialization::context context{};
  context.state.storage.string_view_storage = &storage;

  const caors::error result = caors::read_json_from(context, actor, R"({"name":"Ada"})");

  REQUIRE(result.succeeded());
  REQUIRE(storage.size() == 1);
  CHECK(actor.name == "Ada");
}

// 原测试文件：serialization_builtin_string_view_codec_string_view_storage_keeps_multiple_deserialized_strings_test.cpp

#include "serialization_test_support.h"

#include <string_view>

using namespace caors::tests::serialization;

TEST_CASE("serialization - string view storage keeps multiple deserialized strings",
          "[serialization]") {
  REQUIRE(registration_result_ok(caors::registration::type_<codec_string_view_pair_actor>(
                                     "caors.test.serialization.codec_string_view_pair_actor")
                                     .meta()
                                     .property("first", &codec_string_view_pair_actor::first)
                                     .property("second", &codec_string_view_pair_actor::second)
                                     .serialization()
                                     .commit()));

  codec_string_view_pair_actor actor{};
  caors::serialization::string_storage storage;
  caors::serialization::context context{};
  context.state.storage.string_view_storage = &storage;

  const caors::error result =
      caors::read_json_from(context, actor, R"({"first":"Ada","second":"Mina"})");

  REQUIRE(result.succeeded());
  CHECK(actor.first == "Ada");
  CHECK(actor.second == "Mina");
  CHECK(actor.first.data() != actor.second.data());
  CHECK(storage.size() == 2);
}
