// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

#include <memory>

using namespace caors::tests::serialization;

TEST_CASE("serialization - rejects non-null values for empty raw pointers", "[serialization]") {
  require_pointer_actor_metadata();

  pointer_actor actor{};

  const caors::error result = caors::deserialize_json(
      actor,
      R"({"raw":70,"shared":80,"unique":90,"optional":100,"stats":{"level":4,"score":5.5}})");

  REQUIRE(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::invalid_field);
  CHECK(actor.raw == nullptr);
  CHECK(actor.shared == nullptr);
  CHECK(actor.unique == nullptr);
  CHECK_FALSE(actor.optional.has_value());
  CHECK(actor.stats == nullptr);
}
