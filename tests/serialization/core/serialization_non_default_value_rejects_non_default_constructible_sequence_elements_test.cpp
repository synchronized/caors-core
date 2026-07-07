// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

#include <memory>
#include <variant>

using namespace caors::tests::serialization;

TEST_CASE("serialization - rejects non default constructible sequence elements",
          "[serialization]") {
  require_non_default_value_metadata();

  non_default_vector_actor actor{};
  actor.values.emplace_back(1);

  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"values":[{"value":1}]})");

  const caors::error result = caors::deserialize_json(actor, R"({"values":[{"value":7}]})");

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::invalid_type);
  REQUIRE(actor.values.size() == 1);
  CHECK(actor.values[0].value == 1);
}
