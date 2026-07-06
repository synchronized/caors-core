// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - rejects std tuple length mismatch", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<tuple_actor>("caors.test.serialization.tuple_actor")
          .field("entry", &tuple_actor::entry)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<tuple_actor>()));

  tuple_actor actor{{7, caors::string{"seven"}, 1.5F}};
  const caors::error result = caors::deserialize_json(actor, R"({"entry":[9,"nine"]})");

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::invalid_argument);
  CHECK(std::get<0>(actor.entry) == 7);
  CHECK(std::get<1>(actor.entry) == "seven");
  CHECK(std::get<2>(actor.entry) == 1.5F);
}
