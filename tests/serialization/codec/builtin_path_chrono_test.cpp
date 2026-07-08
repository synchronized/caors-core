// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：serialization_builtin_path_chrono_codec_serialization_built_in_codecs_roundtrip_chrono_values_as_integers_test.cpp

#include "serialization_builtin_path_chrono_codec_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::builtin_path_chrono;
TEST_CASE("serialization - built in codecs roundtrip chrono values as integers",
          "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<codec_chrono_actor>("caors.test.serialization.codec_chrono_actor")
          .field("timeout", &codec_chrono_actor::timeout)
          .field("timestamp", &codec_chrono_actor::timestamp)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<codec_chrono_actor>()));

  codec_chrono_actor actor{
      std::chrono::milliseconds{2500},
      std::chrono::system_clock::time_point{std::chrono::milliseconds{1700000000123}},
  };

  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"timeout":2500,"timestamp":1700000000123})");

  const caors::serialization::variant_result value = caors::to_serialization_value(actor);
  REQUIRE(value.succeeded());
  const caors::value::variant_member* timeout =
      value.value.find_member(caors::string_name{"timeout"});
  const caors::value::variant_member* timestamp =
      value.value.find_member(caors::string_name{"timestamp"});
  REQUIRE(timeout != nullptr);
  REQUIRE(timestamp != nullptr);
  REQUIRE(timeout->value.try_as_i64() != nullptr);
  REQUIRE(timestamp->value.try_as_i64() != nullptr);
  CHECK(*timeout->value.try_as_i64() == 2500);
  CHECK(*timestamp->value.try_as_i64() == 1700000000123);

  codec_chrono_actor decoded{};
  const caors::error result =
      caors::deserialize_json(decoded, R"({"timeout":125,"timestamp":1700000000456})");

  REQUIRE(result.succeeded());
  CHECK(decoded.timeout == std::chrono::milliseconds{125});
  CHECK(decoded.timestamp.time_since_epoch() == std::chrono::milliseconds{1700000000456});
}

// 原测试文件：serialization_builtin_path_chrono_codec_serialization_built_in_codecs_roundtrip_filesystem_paths_as_strings_test.cpp

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

// 原测试文件：serialization_builtin_path_chrono_codec_serialization_built_in_codecs_survive_late_meta_type_registration_test.cpp

#include "serialization_builtin_path_chrono_codec_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::builtin_path_chrono;
TEST_CASE("serialization - built in codecs survive late meta type registration",
          "[serialization]") {
  const std::optional<caors::guid> id = caors::guid::parse("2f6d1f64-7a3a-4db2-9a2d-123456789abc");
  REQUIRE(id.has_value());

  const caors::serialization::variant_result probe =
      caors::serialization::to_variant(caors::any::from(*id));
  REQUIRE(probe.succeeded());
  CHECK_FALSE(caors::meta::find<std::chrono::seconds>());

  require_registered_type<std::chrono::seconds>("std.chrono.seconds");
  REQUIRE(reflection_result_ok(caors::reflection::class_<late_builtin_codec_actor>(
                                   "caors.test.serialization.late_builtin_codec_actor")
                                   .field("timeout", &late_builtin_codec_actor::timeout)
                                   .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<late_builtin_codec_actor>()));

  const late_builtin_codec_actor actor{std::chrono::seconds{42}};
  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);

  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"timeout":42})");

  late_builtin_codec_actor decoded{};
  const caors::error result = caors::deserialize_json(decoded, R"({"timeout":7})");

  REQUIRE(result.succeeded());
  CHECK(decoded.timeout == std::chrono::seconds{7});
}
