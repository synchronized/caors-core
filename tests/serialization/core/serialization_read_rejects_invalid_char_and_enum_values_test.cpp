// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - rejects invalid char and enum values", "[serialization]") {
  require_scalar_actor_metadata();

  scalar_actor actor{};

  const caors::error invalid_char =
      caors::deserialize_json(actor, R"({"code":"AB","color":1,"state":-1})");
  REQUIRE(invalid_char.failed());
  CHECK(invalid_char.domain() == caors::error_domain::serialization);

  const caors::error invalid_enum =
      caors::deserialize_json(actor, R"({"code":"A","color":300,"state":-1})");
  REQUIRE(invalid_enum.failed());
  CHECK(invalid_enum.domain() == caors::error_domain::serialization);
  CHECK(invalid_enum.value() == caors::errc::invalid_argument);
}
