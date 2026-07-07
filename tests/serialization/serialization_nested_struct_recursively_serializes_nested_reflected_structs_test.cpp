// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - recursively serializes nested reflected structs", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<nested_stats>("caors.test.serialization.nested_stats")
          .field("level", &nested_stats::level)
          .field("score", &nested_stats::score)
          .commit()));
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<nested_actor>("caors.test.serialization.nested_actor")
          .field("id", &nested_actor::id)
          .field("stats", &nested_actor::stats)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<nested_stats>()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<nested_actor>()));

  const nested_actor actor{7, nested_stats{3, 12.5F}};
  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);

  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"id":7,"stats":{"level":3,"score":12.5}})");

  const caors::serialization::variant_result value = caors::to_serialization_value(actor);
  REQUIRE(value.succeeded());
  REQUIRE(value.value.is_object());
  const caors::value::variant_member* stats = value.value.find_member(caors::string_name{"stats"});
  REQUIRE(stats != nullptr);
  REQUIRE(stats->value.is_object());
  const caors::value::variant_member* level = stats->value.find_member(caors::string_name{"level"});
  REQUIRE(level != nullptr);
  CHECK(level->value.as_i64() == 3);
}
