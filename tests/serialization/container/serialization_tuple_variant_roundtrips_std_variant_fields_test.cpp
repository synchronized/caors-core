// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - roundtrips std variant fields", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<variant_actor>("caors.test.serialization.variant_actor")
          .field("entry", &variant_actor::entry)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<variant_actor>()));

  variant_actor actor{};
  actor.entry = caors::string{"seven"};

  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"entry":[1,"seven"]})");

  variant_actor decoded{};
  const caors::error result = caors::deserialize_json(decoded, R"({"entry":[2,2.5]})");

  REQUIRE(result.succeeded());
  REQUIRE(std::holds_alternative<float>(decoded.entry));
  CHECK(std::get<float>(decoded.entry) == 2.5F);

  const caors::serialization::variant_result value = caors::to_serialization_value(actor);
  REQUIRE(value.succeeded());
  const caors::value::variant_member* entry = value.value.find_member(caors::string_name{"entry"});
  REQUIRE(entry != nullptr);
  REQUIRE(entry->value.is_array());
  REQUIRE(entry->value.as_array().size() == 2);
  CHECK(entry->value.as_array()[0].as_u64() == 1U);
  CHECK(entry->value.as_array()[1].as_string() == "seven");
}
