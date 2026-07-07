// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

#include <memory>

using namespace caors::tests::serialization;

TEST_CASE("serialization - deserializes non-null values into empty wrappers", "[serialization]") {
  require_pointer_actor_metadata();

  int raw = 0;
  nested_stats stats{};
  pointer_actor actor{};
  actor.raw = &raw;
  actor.stats = &stats;

  const caors::error result = caors::deserialize_json(
      actor,
      R"({"raw":70,"shared":80,"unique":90,"optional":100,"stats":{"level":4,"score":5.5}})");

  REQUIRE(result.succeeded());
  CHECK(actor.raw == &raw);
  REQUIRE(actor.shared != nullptr);
  REQUIRE(actor.unique != nullptr);
  REQUIRE(actor.optional.has_value());
  CHECK(actor.stats == &stats);
  CHECK(raw == 70);
  CHECK(*actor.shared == 80);
  CHECK(*actor.unique == 90);
  CHECK(*actor.optional == 100);
  CHECK(stats.level == 4);
  CHECK(stats.score == Catch::Approx(5.5));
}
