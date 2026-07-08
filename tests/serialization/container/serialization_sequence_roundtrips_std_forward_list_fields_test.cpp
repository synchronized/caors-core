// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - roundtrips std forward list fields", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<forward_list_actor>("caors.test.serialization.forward_list_actor")
          .field("scores", &forward_list_actor::scores)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<forward_list_actor>()));

  forward_list_actor actor{};
  actor.scores.assign({1, 2, 3});

  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"scores":[1,2,3]})");

  forward_list_actor decoded{};
  const caors::error result = caors::deserialize_json(decoded, R"({"scores":[5,6]})");

  REQUIRE(result.succeeded());
  auto iterator = decoded.scores.begin();
  REQUIRE(iterator != decoded.scores.end());
  CHECK(*iterator == 5);
  ++iterator;
  REQUIRE(iterator != decoded.scores.end());
  CHECK(*iterator == 6);
  ++iterator;
  CHECK(iterator == decoded.scores.end());
}
