// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - roundtrips std map fields", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<ordered_map_actor>("caors.test.serialization.ordered_map_actor")
          .field("labels", &ordered_map_actor::labels)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<ordered_map_actor>()));

  ordered_map_actor actor{};
  actor.labels.emplace(2, caors::string{"two"});
  actor.labels.emplace(1, caors::string{"one"});

  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"labels":[[1,"one"],[2,"two"]]})");

  ordered_map_actor decoded{};
  const caors::error result =
      caors::deserialize_json(decoded, R"({"labels":[[3,"three"],[4,"four"]]})");

  REQUIRE(result.succeeded());
  REQUIRE(decoded.labels.size() == 2);
  CHECK(decoded.labels.at(3) == "three");
  CHECK(decoded.labels.at(4) == "four");
}
