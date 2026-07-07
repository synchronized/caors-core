// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

#include <memory>
#include <variant>

using namespace caors::tests::serialization;

TEST_CASE("serialization - updates existing non default constructible unique pointers",
          "[serialization]") {
  require_non_default_value_metadata();

  non_default_unique_ptr_actor actor{};
  actor.value = std::make_unique<non_default_value>(1);

  const caors::error result = caors::deserialize_json(actor, R"({"value":{"value":7}})");

  REQUIRE(result.succeeded());
  REQUIRE(actor.value != nullptr);
  CHECK(actor.value->value == 7);
}
