// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

#include <memory>

using namespace caors::tests::serialization;

TEST_CASE("serialization - rejects null reference wrapper fields", "[serialization]") {
  require_reference_wrapper_actor_metadata();

  reference_wrapper_actor actor{7};
  const caors::error result = caors::deserialize_json(actor, R"({"value":null})");

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::invalid_field);
  CHECK(actor.storage == 7);
}
