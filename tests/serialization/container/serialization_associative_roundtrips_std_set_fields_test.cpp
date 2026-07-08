// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - roundtrips std set fields", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<ordered_set_actor>("caors.test.serialization.ordered_set_actor")
          .field("ids", &ordered_set_actor::ids)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<ordered_set_actor>()));

  ordered_set_actor actor{};
  actor.ids.emplace(3);
  actor.ids.emplace(1);

  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"ids":[1,3]})");

  ordered_set_actor decoded{};
  const caors::error result = caors::deserialize_json(decoded, R"({"ids":[2,4]})");

  REQUIRE(result.succeeded());
  REQUIRE(decoded.ids.size() == 2);
  CHECK(decoded.ids.contains(2));
  CHECK(decoded.ids.contains(4));
}
