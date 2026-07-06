// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - roundtrips std pair fields", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<pair_actor>("caors.test.serialization.pair_actor")
          .field("entry", &pair_actor::entry)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<pair_actor>()));

  pair_actor actor{{7, caors::string{"seven"}}};

  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"entry":[7,"seven"]})");

  pair_actor decoded{};
  const caors::error result = caors::deserialize_json(decoded, R"({"entry":[9,"nine"]})");

  REQUIRE(result.succeeded());
  CHECK(decoded.entry.first == 9);
  CHECK(decoded.entry.second == "nine");

  const caors::serialization::variant_result value = caors::to_serialization_value(actor);
  REQUIRE(value.succeeded());
  const caors::value::variant_member* entry = value.value.find_member(caors::string_name{"entry"});
  REQUIRE(entry != nullptr);
  REQUIRE(entry->value.is_array());
  REQUIRE(entry->value.as_array().size() == 2);
  CHECK(entry->value.as_array()[0].as_i64() == 7);
  CHECK(entry->value.as_array()[1].as_string() == "seven");
}
