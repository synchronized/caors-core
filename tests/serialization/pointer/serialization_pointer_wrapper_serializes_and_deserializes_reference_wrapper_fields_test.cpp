// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

#include <memory>

using namespace caors::tests::serialization;

TEST_CASE("serialization - serializes and deserializes reference wrapper fields",
          "[serialization]") {
  require_reference_wrapper_actor_metadata();

  reference_wrapper_actor actor{7};

  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"value":7})");

  const caors::error result = caors::deserialize_json(actor, R"({"value":11})");

  REQUIRE(result.succeeded());
  CHECK(&actor.value.get() == &actor.storage);
  CHECK(actor.storage == 11);
}
