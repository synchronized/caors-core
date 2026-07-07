// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - roundtrips std tuple fields", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<tuple_actor>("caors.test.serialization.tuple_actor")
          .field("entry", &tuple_actor::entry)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<tuple_actor>()));

  tuple_actor actor{{7, caors::string{"seven"}, 1.5F}};

  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"entry":[7,"seven",1.5]})");

  tuple_actor decoded{};
  const caors::error result = caors::deserialize_json(decoded, R"({"entry":[9,"nine",2.5]})");

  REQUIRE(result.succeeded());
  CHECK(std::get<0>(decoded.entry) == 9);
  CHECK(std::get<1>(decoded.entry) == "nine");
  CHECK(std::get<2>(decoded.entry) == 2.5F);

  const caors::serialization::variant_result value = caors::to_serialization_value(actor);
  REQUIRE(value.succeeded());
  const caors::value::variant_member* entry = value.value.find_member(caors::string_name{"entry"});
  REQUIRE(entry != nullptr);
  REQUIRE(entry->value.is_array());
  REQUIRE(entry->value.as_array().size() == 3);
  CHECK(entry->value.as_array()[0].as_i64() == 7);
  CHECK(entry->value.as_array()[1].as_string() == "seven");
  CHECK(entry->value.as_array()[2].as_f64() == 1.5);
}
