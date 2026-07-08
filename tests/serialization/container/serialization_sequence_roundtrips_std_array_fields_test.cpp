// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - roundtrips std array fields", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<array_actor>("caors.test.serialization.array_actor")
          .field("scores", &array_actor::scores)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<array_actor>()));

  array_actor actor{{1, 2, 3}};

  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"scores":[1,2,3]})");

  array_actor decoded{};
  const caors::error result = caors::deserialize_json(decoded, R"({"scores":[5,6,7]})");

  REQUIRE(result.succeeded());
  CHECK(decoded.scores == std::array<int, 3>{5, 6, 7});
}
