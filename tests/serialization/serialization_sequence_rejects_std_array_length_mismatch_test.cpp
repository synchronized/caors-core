// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - rejects std array length mismatch", "[serialization]") {
  using scores_type = std::array<int, 3>;
  require_registered_type<scores_type>("std.array<int,3>");
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<array_actor>("caors.test.serialization.array_actor")
          .field("scores", &array_actor::scores)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<array_actor>()));

  array_actor actor{{1, 2, 3}};
  const caors::error result = caors::deserialize_json(actor, R"({"scores":[5,6]})");

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::invalid_argument);
  CHECK(actor.scores == std::array<int, 3>{1, 2, 3});
}
