// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

#include <memory>
#include <variant>

using namespace caors::tests::serialization;

TEST_CASE("serialization - updates non default constructible direct fields", "[serialization]") {
  require_non_default_value_metadata();

  non_default_field_actor actor{non_default_value{1}};

  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"value":{"value":1}})");

  const caors::error result = caors::deserialize_json(actor, R"({"value":{"value":7}})");

  REQUIRE(result.succeeded());
  CHECK(actor.value.value == 7);
}
