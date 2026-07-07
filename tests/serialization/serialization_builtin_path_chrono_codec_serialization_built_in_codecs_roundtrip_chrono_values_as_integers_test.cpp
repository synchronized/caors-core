// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

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
