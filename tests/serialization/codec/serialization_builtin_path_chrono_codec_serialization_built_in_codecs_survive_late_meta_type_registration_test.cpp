// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

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
