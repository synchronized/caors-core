// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - rejects std variant invalid data", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<variant_actor>("caors.test.serialization.variant_actor")
          .field("entry", &variant_actor::entry)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<variant_actor>()));

  variant_actor actor{};
  actor.entry = caors::string{"seven"};
  const caors::error invalid_index = caors::deserialize_json(actor, R"({"entry":[9,"bad"]})");

  CHECK(invalid_index.failed());
  CHECK(invalid_index.domain() == caors::error_domain::serialization);
  CHECK(invalid_index.value() == caors::errc::invalid_argument);
  REQUIRE(std::holds_alternative<caors::string>(actor.entry));
  CHECK(std::get<caors::string>(actor.entry) == "seven");

  const caors::error invalid_length = caors::deserialize_json(actor, R"({"entry":[1]})");

  CHECK(invalid_length.failed());
  CHECK(invalid_length.domain() == caors::error_domain::serialization);
  CHECK(invalid_length.value() == caors::errc::invalid_argument);
  REQUIRE(std::holds_alternative<caors::string>(actor.entry));
  CHECK(std::get<caors::string>(actor.entry) == "seven");
}
