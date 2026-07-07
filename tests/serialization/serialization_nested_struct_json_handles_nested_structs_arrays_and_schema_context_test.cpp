// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - json handles nested structs arrays and schema context",
          "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<nested_stats>("caors.test.serialization.nested_stats")
          .field("level", &nested_stats::level)
          .field("score", &nested_stats::score)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<nested_stats>()));
  require_registered_type<caors::vector<nested_stats>>(
      "caors.vector<caors.test.serialization.nested_stats>");
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<nested_vector_actor>("caors.test.serialization.nested_vector_actor")
          .field("stats", &nested_vector_actor::stats)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<nested_vector_actor>()));

  nested_vector_actor actor{};
  actor.stats.push_back(nested_stats{1, 2.5F});
  actor.stats.push_back(nested_stats{3, 4.5F});

  const caors::serialization::serialization_text_result json = caors::serialize_json(actor);
  REQUIRE(json.succeeded());
  CHECK(json.value == R"({"stats":[{"level":1,"score":2.5},{"level":3,"score":4.5}]})");
}
