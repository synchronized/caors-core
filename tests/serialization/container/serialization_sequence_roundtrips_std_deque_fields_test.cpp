// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - roundtrips std deque fields", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<deque_actor>("caors.test.serialization.deque_actor")
          .field("scores", &deque_actor::scores)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<deque_actor>()));

  deque_actor actor{};
  actor.scores.push_back(1);
  actor.scores.push_back(2);
  actor.scores.push_back(3);

  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"scores":[1,2,3]})");

  deque_actor decoded{};
  const caors::error result = caors::deserialize_json(decoded, R"({"scores":[5,6]})");

  REQUIRE(result.succeeded());
  REQUIRE(decoded.scores.size() == 2);
  CHECK(decoded.scores[0] == 5);
  CHECK(decoded.scores[1] == 6);
}
