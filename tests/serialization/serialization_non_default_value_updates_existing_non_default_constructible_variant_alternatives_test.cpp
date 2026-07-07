// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

#include <memory>
#include <variant>

using namespace caors::tests::serialization;

TEST_CASE("serialization - updates existing non default constructible variant alternatives",
          "[serialization]") {
  require_non_default_value_metadata();

  non_default_variant_actor actor{};
  actor.value.emplace<1>(1);

  const caors::error result = caors::deserialize_json(actor, R"({"value":[1,{"value":7}]})");

  REQUIRE(result.succeeded());
  REQUIRE(std::holds_alternative<non_default_value>(actor.value));
  CHECK(std::get<non_default_value>(actor.value).value == 7);
}
