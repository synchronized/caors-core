// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_builtin_path_chrono_codec_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::builtin_path_chrono;
TEST_CASE("serialization - built in codecs roundtrip filesystem paths as strings",
          "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<codec_path_actor>("caors.test.serialization.codec_path_actor")
          .field("path", &codec_path_actor::path)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<codec_path_actor>()));

  codec_path_actor actor{std::filesystem::path{"assets/textures/albedo.png"}};

  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"path":"assets/textures/albedo.png"})");

  const caors::serialization::variant_result value = caors::to_serialization_value(actor);
  REQUIRE(value.succeeded());
  const caors::value::variant_member* path = value.value.find_member(caors::string_name{"path"});
  REQUIRE(path != nullptr);
  REQUIRE(path->value.try_as_string() != nullptr);
  CHECK(*path->value.try_as_string() == "assets/textures/albedo.png");

  codec_path_actor decoded{};
  const caors::error result = caors::deserialize_json(decoded, R"({"path":"levels/start.scene"})");

  REQUIRE(result.succeeded());
  CHECK(decoded.path.generic_string() == "levels/start.scene");
}
