// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

#include <memory>
#include <variant>

using namespace caors::tests::serialization;

TEST_CASE("serialization - rejects switching to non default constructible variant alternatives",
          "[serialization]") {
  require_non_default_value_metadata();

  non_default_variant_actor actor{};
  actor.value = 3;

  const caors::error result = caors::deserialize_json(actor, R"({"value":[1,{"value":7}]})");

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::invalid_type);
  REQUIRE(std::holds_alternative<int>(actor.value));
  CHECK(std::get<int>(actor.value) == 3);
}
