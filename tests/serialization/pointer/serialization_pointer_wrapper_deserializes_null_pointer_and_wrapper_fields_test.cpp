// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

#include <memory>

using namespace caors::tests::serialization;

TEST_CASE("serialization - deserializes null pointer and wrapper fields", "[serialization]") {
  require_pointer_actor_metadata();

  int raw = 1;
  nested_stats stats{1, 2.0F};
  pointer_actor actor{};
  actor.raw = &raw;
  actor.shared = std::make_shared<int>(8);
  actor.unique = std::make_unique<int>(9);
  actor.optional = 10;
  actor.stats = &stats;

  const caors::error result = caors::deserialize_json(
      actor, R"({"raw":null,"shared":null,"unique":null,"optional":null,"stats":null})");

  REQUIRE(result.succeeded());
  CHECK(actor.raw == nullptr);
  CHECK(actor.shared == nullptr);
  CHECK(actor.unique == nullptr);
  CHECK_FALSE(actor.optional.has_value());
  CHECK(actor.stats == nullptr);
}
