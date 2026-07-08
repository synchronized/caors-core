// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - serializes hash map fields as dictionaries", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<map_actor>("caors.test.serialization.map_actor")
          .field("labels", &map_actor::labels)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<map_actor>()));

  map_actor actor{};
  actor.labels.emplace(7, caors::string{"seven"});

  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"labels":[[7,"seven"]]})");

  const caors::serialization::variant_result value = caors::to_serialization_value(actor);
  REQUIRE(value.succeeded());
  const caors::value::variant_member* labels =
      value.value.find_member(caors::string_name{"labels"});
  REQUIRE(labels != nullptr);
  REQUIRE(labels->value.is_dictionary());
  REQUIRE(labels->value.as_dictionary().size() == 1);
  CHECK(labels->value.as_dictionary()[0].key.as_i64() == 7);
  CHECK(labels->value.as_dictionary()[0].value.as_string() == "seven");
}
