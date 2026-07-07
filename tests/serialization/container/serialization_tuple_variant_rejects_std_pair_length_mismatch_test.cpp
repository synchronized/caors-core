// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - rejects std pair length mismatch", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<pair_actor>("caors.test.serialization.pair_actor")
          .field("entry", &pair_actor::entry)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<pair_actor>()));

  pair_actor actor{{7, caors::string{"seven"}}};
  const caors::error result = caors::deserialize_json(actor, R"({"entry":[9]})");

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::invalid_argument);
  CHECK(actor.entry.first == 7);
  CHECK(actor.entry.second == "seven");
}
